/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "common.h"
#include "global.h"
#include "vfs_so.h"
#include "myepoll.h"
#include "protocol.h"
#include "util.h"
#include "acl.h"
#include "list.h"
#include "global.h"
#include "vfs_init.h"
#include "vfs_task.h"
#include "common.h"

typedef struct {
	uint8_t  type;
	char 	 filename[255];
	char     filemd5[36];
	char	 srcip[16];
	uint64_t datalen;
	off_t    start;
	off_t    end;
} t_uc_oss_http_header;

typedef struct {
	list_head_t alist;
	char fname[256];
	t_uc_oss_http_header header;
	int fd;
	uint32_t hbtime;
	int nostandby; // 1: delay process 
} http_peer;

int vfs_http_log = -1;
static list_head_t activelist;  //ÓÃÀ´¼ì²â³¬Ê±

static int do_req(int cfd, t_uc_oss_http_header *header)
{
	char httpheader[256] = {0};
	int fd;
	struct stat st;
	
	fd = open(header->filename, O_RDONLY);
	if(fd > 0) 
	{
		fstat(fd, &st);
		off_t len = 0;
		if (header->start > st.st_size - 1 || header->start > header->end)
		{
			sprintf(httpheader, "HTTP/1.1 416 RANGEERROR\r\nContent-Type: video/x-flv\r\nContent-Length: 0\r\n\r\n");
			set_client_data(cfd, httpheader, strlen(httpheader));
		}

		if (header->end > st.st_size - 1)
			header->end = st.st_size - 1;
		len =  header->end - header->start + 1;
		sprintf(httpheader, "HTTP/1.1 200 OK\r\nContent-Type: video/x-flv\r\nContent-Length: %ld\r\n\r\n", len);
		set_client_data(cfd, httpheader, strlen(httpheader));
		set_client_fd(cfd, fd, header->start, len);
	}
	else
	{
		LOG(vfs_http_log, LOG_ERROR, "open %s err %m\n", header->filename);
		sprintf(httpheader, "HTTP/1.1 404 NOTFOUND\r\nContent-Type: video/x-flv\r\nContent-Length: 0\r\n\r\n");
		set_client_data(cfd, httpheader, strlen(httpheader));
	}
	return 0;
}

int svc_init() 
{
	char *logname = myconfig_get_value("log_server_logname");
	if (!logname)
		logname = "../log/http_log.log";

	char *cloglevel = myconfig_get_value("log_server_loglevel");
	int loglevel = LOG_DEBUG;
	if (cloglevel)
		loglevel = getloglevel(cloglevel);
	int logsize = myconfig_get_intval("log_server_logsize", 100);
	int logintval = myconfig_get_intval("log_server_logtime", 3600);
	int lognum = myconfig_get_intval("log_server_lognum", 10);
	vfs_http_log = registerlog(logname, loglevel, logsize, logintval, lognum);
	if (vfs_http_log < 0)
		return -1;
	INIT_LIST_HEAD(&activelist);
	LOG(vfs_http_log, LOG_DEBUG, "svc_init init log ok!\n");
	return 0;
}

int svc_initconn(int fd) 
{
	LOG(vfs_http_log, LOG_DEBUG, "%s:%s:%d\n", ID, FUNC, LN);
	struct conn *curcon = &acon[fd];
	if (curcon->user == NULL)
		curcon->user = malloc(sizeof(http_peer));
	if (curcon->user == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "malloc err %m\n");
		return RET_CLOSE_MALLOC;
	}
	memset(curcon->user, 0, sizeof(http_peer));
	http_peer * peer = (http_peer *)curcon->user;
	peer->hbtime = time(NULL);
	peer->fd = fd;
	ip2str(peer->header.srcip, getpeerip(fd));
	INIT_LIST_HEAD(&(peer->alist));
	list_move_tail(&(peer->alist), &activelist);
	LOG(vfs_http_log, LOG_DEBUG, "a new fd[%d] init ok!\n", fd);
	return 0;
}

static char * parse_item(char *src, char *item, char **end)
{
	char *p = strstr(src, item);
	if (p == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "data[%s] no [%s]!\n", src, item);
		return NULL;
	}

	p += strlen(item);
	char *e = strstr(p, "\r\n");
	if (e == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "data[%s] no [%s] end!\n", src, item);
		return NULL;
	}
	*e = 0x0;

	*end = e + 2;

	return p;
}

static int parse_header(t_uc_oss_http_header *peer, char *data, int maxlen)
{
	char *end = NULL;

	char *pret = parse_item(data, "filename: ", &end);
	if (pret == NULL)
		return -1;
	snprintf(peer->filename, sizeof(peer->filename), "%s", pret);
	data = end;

	pret = parse_item(data, "type: ", &end);
	if (pret == NULL)
		return -1;
	peer->type = atoi(pret);
	data = end;

	pret = parse_item(data, "datalen: ", &end);
	if (pret == NULL)
		return -1;
	peer->datalen = atol(pret);
	data = end;

	pret = parse_item(data, "start: ", &end);
	if (pret == NULL)
		return -1;
	peer->start = atol(pret);
	data = end;

	pret = parse_item(data, "end: ", &end);
	if (pret == NULL)
		return -1;
	peer->end = atol(pret);
	data = end;

	pret = parse_item(data, "filemd5: ", &end);
	if (pret == NULL)
		return -1;
	snprintf(peer->filemd5, sizeof(peer->filemd5), "%s", pret);

	return 0;
}

static int check_request(int fd, char* data, int len) 
{
	if(len < 14)
		return 0;

	struct conn *c = &acon[fd];
	http_peer *peer = (http_peer *) c->user;
	if(!strncmp(data, "GET /", 5)) {
		char* p;
		if((p = strstr(data + 5, "\r\n\r\n")) != NULL) {
			LOG(vfs_http_log, LOG_DEBUG, "fd[%d] data[%s]!\n", fd, data);
			char* q;
			int len;
			if((q = strstr(data + 5, " HTTP/")) != NULL) {
				len = q - data - 5;
				if(len < 1023) {
					if (parse_header(&(peer->header), data, p - data))
						return -1;
					do_req(fd, &(peer->header));
					return p - data + 4;
				}
			}
			return -2;	
		}
		else
			return 0;
	}
	else
		return -1;
}

static int check_req(int fd)
{
	char *data;
	size_t datalen;
	if (get_client_data(fd, &data, &datalen))
	{
		LOG(vfs_http_log, LOG_DEBUG, "fd[%d] no data!\n", fd);
		return RECV_ADD_EPOLLIN;  /*no suffic data, need to get data more */
	}
	int clen = check_request(fd, data, datalen);
	if (clen < 0)
	{
		LOG(vfs_http_log, LOG_DEBUG, "fd[%d] data error ,not http!\n", fd);
		return RECV_CLOSE;
	}
	if (clen == 0)
	{
		LOG(vfs_http_log, LOG_DEBUG, "fd[%d] data not suffic!\n", fd);
		return RECV_ADD_EPOLLIN;
	}
	consume_client_data(fd, clen);
	return RECV_SEND;
}

int svc_recv(int fd) 
{
	return check_req(fd);
}

int svc_send(int fd)
{
	struct conn *curcon = &acon[fd];
	http_peer *peer = (http_peer *) curcon->user;
	peer->hbtime = time(NULL);
	return SEND_ADD_EPOLLIN;
}

void svc_timeout()
{
	time_t now = time(NULL);
	http_peer *peer = NULL;
	list_head_t *l;
	list_for_each_entry_safe_l(peer, l, &activelist, alist)
	{
		if (peer == NULL)
			continue;   /*bugs */
		if (now - peer->hbtime > g_config.timeout)
			do_close(peer->fd);
	}
}

void svc_finiconn(int fd)
{
	LOG(vfs_http_log, LOG_DEBUG, "close %d\n", fd);
	struct conn *curcon = &acon[fd];
	if (curcon->user == NULL)
		return;
	http_peer *peer = (http_peer *) curcon->user;
	list_del_init(&(peer->alist));
}
