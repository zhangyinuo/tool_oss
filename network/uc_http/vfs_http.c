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
#include <libgen.h>
#include <sys/syscall.h>
#include "common.h"
#include "global.h"
#include "vfs_so.h"
#include "vfs_localfile.h"
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
	list_head_t alist;
	char fname[256];
	t_uc_oss_http_header header;
	int fd;
	uint32_t hbtime;
	int nostandby; // 1: delay process 
} http_peer;

extern char hostname[128];

int vfs_http_log = -1;
static list_head_t activelist;  //ÓÃÀ´¼ì²â³¬Ê±

static int insert_sub_task(t_task_base *base0, int idx, int count, off_t start, off_t end)
{
	snprintf(base0->hostname, sizeof(base0->hostname), "%s", hostname);
	t_vfs_tasklist *task = NULL;
	int ret = vfs_get_task(&task, TASK_HOME);
	if (ret != GET_TASK_OK)
	{
		LOG(vfs_http_log, LOG_ERROR, "vfs_get_task err %m %s\n", base0->filename);
		return -1;
	}

	t_task_base *base = (t_task_base *) &(task->task.base);
	t_task_sub *sub = (t_task_sub *) &(task->task.sub);
	memset(base, 0, sizeof(t_task_base));
	memset(sub, 0, sizeof(t_task_sub));

	sub->idx = idx;
	sub->count = count;

	sub->start = start;
	sub->end = end;
	sub->starttime = time(NULL);
	memcpy(base, base0, sizeof(t_task_base));
	base->stime = sub->starttime;

	LOG(vfs_http_log, LOG_NORMAL, "%s %s:%s:%s %ld:%ld:%d:%d\n", __FUNCTION__, base->srcip, base->filename, base->filemd5, sub->start, sub->end, sub->idx, sub->count);
	vfs_set_task(task, TASK_WAIT_SYNC);
	return 0;
}

static void process_lack(char *filename, int idx, int count)
{
	t_task_base base;
	memset(&base, 0, sizeof(t_task_base));
	snprintf(base.filename, sizeof(base.filename), "%s", filename);
	if (get_localfile_stat(&base))
	{
		LOG(vfs_http_log, LOG_ERROR, "get_localfile_stat err %m %s\n", base.filename);
		return;
	}
	LOG(vfs_http_log, LOG_NORMAL, "process %s:%d:%d\n", base.filename, idx, count);
	if (idx < 1 || count < 1 || idx > count)
	{
		LOG(vfs_http_log, LOG_ERROR, "idx err %s:%d:%d\n", base.filename, idx, count);
		return;
	}

	off_t start = g_config.splic_min_size * (idx - 1);
	off_t end = start + g_config.splic_min_size - 1;
	if (end >  base.fsize - 1)
		end = base.fsize - 1;

	if (start < 0 || start > base.fsize - 1)
	{
		LOG(vfs_http_log, LOG_ERROR, "err start %s:%d:%d\n", base.filename, idx, count);
		return;
	}
	insert_sub_task(&base, idx, count, start, end);
}

static int do_req(char *fname)
{
	LOG(vfs_http_log, LOG_NORMAL, "do_req process %s\n", fname);
	char *s = fname;
	while (1)
	{
		char *t = strchr(s, ':');
		if (t == NULL)
			break;
		*t = 0x0;
		char rect[4][256];
		memset(rect, 0, sizeof(rect));
		int n = sscanf(s, "%[^ ] %[^ ] %[^ ]", rect[0], rect[1], rect[2]);
		if (n == 3)
			process_lack(rect[0], atoi(rect[1]), atoi(rect[2]));
		else
			LOG(vfs_http_log, LOG_ERROR, "err %s %d\n", s, n);
		s = t + 1;
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

#include "vfs_http_base.c"

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
					do_req(peer->header.filename);
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
	static time_t last_check = 0;
	if (now - last_check < 30)
		return;
	last_check = now;
	check_task();
}

void svc_finiconn(int fd)
{
	LOG(vfs_http_log, LOG_DEBUG, "close %d\n", fd);
	struct conn *curcon = &acon[fd];
	if (curcon->user == NULL)
		return;
	http_peer *peer = (http_peer *) curcon->user;
	list_del_init(&(peer->alist));
	memset(curcon->user, 0, sizeof(http_peer));
	free(curcon->user);
	curcon->user = NULL;
}
