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
#include "vfs_data.h"
#include "util.h"
#include "acl.h"
#include "vfs_task.h"
#include "vfs_data_task.h"
#include "vfs_localfile.h"

int vfs_sig_log = -1;
extern uint8_t self_stat ;
extern t_ip_info self_ipinfo;
/* online list */
static list_head_t activelist;  //用来检测超时
static list_head_t online_list[256]; //用来快速定位查找

int g_proxyed = 0;
t_vfs_up_proxy g_proxy;
int svc_initconn(int fd); 
int active_send(int fd, char *data);
const char *sock_stat_cmd[] = {"LOGOUT", "CONNECTED", "LOGIN", "IDLE", "PREPARE_RECVFILE", "RECVFILEING", "SENDFILEING", "LAST_STAT"};

#include "vfs_data_base.c"
#include "vfs_data_sub.c"

static int init_proxy_info()
{
	return 0;
}

int svc_init() 
{
	char *logname = myconfig_get_value("log_data_logname");
	if (!logname)
		logname = "./data_log.log";

	char *cloglevel = myconfig_get_value("log_data_loglevel");
	int loglevel = LOG_NORMAL;
	if (cloglevel)
		loglevel = getloglevel(cloglevel);
	int logsize = myconfig_get_intval("log_data_logsize", 100);
	int logintval = myconfig_get_intval("log_data_logtime", 3600);
	int lognum = myconfig_get_intval("log_data_lognum", 10);
	vfs_sig_log = registerlog(logname, loglevel, logsize, logintval, lognum);
	if (vfs_sig_log < 0)
		return -1;
	LOG(vfs_sig_log, LOG_NORMAL, "svc_init init log ok!\n");
	INIT_LIST_HEAD(&activelist);
	int i = 0;
	for (i = 0; i < 256; i++)
	{
		INIT_LIST_HEAD(&online_list[i]);
	}
	if (init_proxy_info())
	{
		LOG(vfs_sig_log, LOG_ERROR, "init_proxy_info err!\n");
		return -1;
	}
	if (g_proxyed)
		LOG(vfs_sig_log, LOG_NORMAL, "proxy mode!\n");
	else
		LOG(vfs_sig_log, LOG_NORMAL, "not proxy mode!\n");
	
	return 0;
}

int svc_initconn(int fd) 
{
	uint32_t ip = getpeerip(fd);
	LOG(vfs_sig_log, LOG_TRACE, "%s:%s:%d\n", ID, FUNC, LN);
	struct conn *curcon = &acon[fd];
	if (curcon->user == NULL)
		curcon->user = malloc(sizeof(vfs_cs_peer));
	if (curcon->user == NULL)
	{
		LOG(vfs_sig_log, LOG_ERROR, "malloc err %m\n");
		char val[256] = {0x0};
		snprintf(val, sizeof(val), "malloc err %m");
		SetStr(VFS_MALLOC, val);
		return RET_CLOSE_MALLOC;
	}
	vfs_cs_peer *peer;
	memset(curcon->user, 0, sizeof(vfs_cs_peer));
	peer = (vfs_cs_peer *)curcon->user;
	peer->hbtime = time(NULL);
	peer->sock_stat = CONNECTED;
	peer->fd = fd;
	peer->ip = ip;
	INIT_LIST_HEAD(&(peer->alist));
	INIT_LIST_HEAD(&(peer->hlist));
	list_move_tail(&(peer->alist), &activelist);
	list_add_head(&(peer->hlist), &online_list[ip&ALLMASK]);
	LOG(vfs_sig_log, LOG_TRACE, "a new fd[%d] init ok!\n", fd);
	return 0;
}

static void convert_httpheader_2_task(t_uc_oss_http_header *header, t_task_base *base, t_task_sub *sub)
{
	base->type = header->type;
	snprintf(base->filemd5, sizeof(base->filemd5), "%s", header->filemd5);
	snprintf(base->filename, sizeof(base->filename), "%s", header->filename);
	snprintf(base->hostname, sizeof(base->hostname), "%s", header->hostname);

	sub->idx = header->idx;
	sub->count = header->count;

	sub->start = header->start;
	sub->end= header->end;
}

static char * parse_item(char *src, char *item, char **end)
{
	char *p = strstr(src, item);
	if (p == NULL)
	{
		LOG(vfs_sig_log, LOG_ERROR, "data[%s] no [%s]!\n", src, item);
		return NULL;
	}

	p += strlen(item);
	char *e = strstr(p, "\r\n");
	if (e == NULL)
	{
		LOG(vfs_sig_log, LOG_ERROR, "data[%s] no [%s] end!\n", src, item);
		return NULL;
	}
	*e = 0x0;

	*end = e + 2;

	return p;
}

static int parse_header(t_uc_oss_http_header *peer, char *data)
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
	data = end;

	pret = parse_item(data, "idx: ", &end);
	if (pret == NULL)
		return -1;
	peer->idx = atol(pret);
	data = end;

	pret = parse_item(data, "count: ", &end);
	if (pret == NULL)
		return -1;
	peer->count = atol(pret);
	data = end;

	pret = parse_item(data, "hostname: ", &end);
	if (pret == NULL)
		return -1;
	snprintf(peer->hostname, sizeof(peer->hostname), "%s", pret);

	return 0;
}

static int check_req(int fd)
{
	LOG(vfs_sig_log, LOG_DEBUG, "%s:%s:%d\n", ID, FUNC, LN);
	char *data;
	size_t datalen;
	if (get_client_data(fd, &data, &datalen))
	{
		LOG(vfs_sig_log, LOG_TRACE, "%s:%d fd[%d] no data!\n", FUNC, LN, fd);
		return -1;  /*no suffic data, need to get data more */
	}
	char *end = strstr(data, "\r\n\r\n");
	if (end == NULL)
		return -1;
	end += 4;

	struct conn *curcon = &acon[fd];
	vfs_cs_peer *peer = (vfs_cs_peer *) curcon->user;
	peer->sock_stat = RECV_HEAD_ING;

	t_uc_oss_http_header *header = (t_uc_oss_http_header *) &(peer->header);
	if (parse_header(header, data))
	{
		if (header->type != 250)
		{
			LOG(vfs_sig_log, LOG_ERROR, "%s:%d fd[%d] ERROR parse_header error!\n", FUNC, LN, fd);
			return RECV_CLOSE;
		}
	}

	if (header->type == 250)
	{
		char taskbuf[10240] = {0x0};
		int tasklen = get_task_unok(taskbuf, header->hostname);
		set_client_data(fd, taskbuf, tasklen);
		LOG(vfs_sig_log, LOG_NORMAL, "%s:%d fd[%d] get_task_unok !\n", FUNC, LN, fd);
		return RECV_SEND;
	}

	t_vfs_tasklist *task = NULL;
	int ret = vfs_get_task(&task, TASK_HOME);
	if (ret != GET_TASK_OK)
	{
		LOG(vfs_sig_log, LOG_ERROR, "vfs_get_task err %m %s:%s\n", header->srcip, header->filename);
		return -1;
	}

	t_task_base *base = (t_task_base *) &(task->task.base);
	t_task_sub *sub = (t_task_sub *) &(task->task.sub);
	memset(base, 0, sizeof(t_task_base));
	memset(sub, 0, sizeof(t_task_sub));

	base->usrcip = peer->ip;
	convert_httpheader_2_task(header, base, sub);
	peer->recvtask = task;

	int clen = end - data;
	off_t content_length = header->end - header->start + 1;
	ret = do_req(fd, content_length);
	LOG(vfs_sig_log, LOG_DEBUG, "%s:%d fd[%d] Content-Length: %ld!\n", FUNC, LN, fd, content_length);
	consume_client_data(fd, clen);
	return ret;
}

int svc_recv(int fd) 
{
	char val[256] = {0x0};
	struct conn *curcon = &acon[fd];
	vfs_cs_peer *peer = (vfs_cs_peer *) curcon->user;
recvfileing:
	peer->hbtime = time(NULL);
	list_move_tail(&(peer->alist), &activelist);
	LOG(vfs_sig_log, LOG_TRACE, "fd[%d] sock stat %d!\n", fd, peer->sock_stat);
	if (peer->sock_stat == RECV_BODY_ING)
	{
		LOG(vfs_sig_log, LOG_TRACE, "%s:%s:%d\n", ID, FUNC, LN);
		t_vfs_tasklist *task0 = peer->recvtask;
		char *data;
		size_t datalen;
		if(task0 == NULL)
		{
			LOG(vfs_sig_log, LOG_ERROR, "recv task is null!\n");
			return RECV_CLOSE;  /* ERROR , close it */
		}
		t_vfs_taskinfo *task = &(peer->recvtask->task);
		if (get_client_data(fd, &data, &datalen))
		{
			LOG(vfs_sig_log, LOG_TRACE, "%s:%d fd[%d] no data!\n", FUNC, LN, fd);
			return RECV_ADD_EPOLLIN;  /*no suffic data, need to get data more */
		}
		LOG(vfs_sig_log, LOG_TRACE, "fd[%d] get file %s:%d!\n", fd, task->base.filename, datalen);
		int remainlen = task->base.fsize - task->base.getlen;
		datalen = datalen <= remainlen ? datalen : remainlen ; 
		int n = write(peer->local_in_fd, data, datalen);
		if (n < 0)
		{
			LOG(vfs_sig_log, LOG_ERROR, "fd[%d] write error %m close it!\n", fd);
			snprintf(val, sizeof(val), "write err %m");
			SetStr(VFS_WRITE_FILE, val);
			return RECV_CLOSE;  /* ERROR , close it */
		}
		consume_client_data(fd, n);
		task->base.getlen += n;
		if (task->base.getlen >= task->base.fsize)
		{
			if (close_tmp_check_mv(&(task->base), peer->local_in_fd) != LOCALFILE_OK)
			{
				LOG(vfs_sig_log, LOG_ERROR, "fd[%d] get file %s error!\n", fd, task->base.tmpfile);
				task0->task.base.overstatus = OVER_E_MD5;
				peer->recvtask = NULL;
				vfs_set_task(task0, TASK_FIN);
				snprintf(val, sizeof(val), "md5 error %m");
				SetStr(VFS_STR_MD5_E, val);
				return RECV_CLOSE;
			}
			else
			{
				LOG(vfs_sig_log, LOG_NORMAL, "fd[%d:%u] get file %s ok!\n", fd, peer->ip, task->base.tmpfile);
				task0->task.base.overstatus = OVER_OK;
				vfs_set_task(task0, TASK_FIN);
			}
			peer->local_in_fd = -1;
			peer->recvtask = NULL;
			return RECV_CLOSE;
		}
		else
			return RECV_ADD_EPOLLIN;  /*no suffic data, need to get data more */
	}
	
	int ret = RECV_ADD_EPOLLIN;;
	int subret = 0;
	while (1)
	{
		subret = check_req(fd);
		if (subret == -1)
			break;
		if (subret == RECV_SEND)
			return subret;
		if (subret == RECV_CLOSE)
		{
			peer->recvtask = NULL;
			return RECV_CLOSE;
		}
		if (peer->sock_stat == RECV_BODY_ING)
			goto recvfileing;
	}
	return ret;
}

int svc_send_once(int fd)
{
	struct conn *curcon = &acon[fd];
	vfs_cs_peer *peer = (vfs_cs_peer *) curcon->user;
	peer->hbtime = time(NULL);
	return SEND_ADD_EPOLLIN;
}

int svc_send(int fd)
{
	struct conn *curcon = &acon[fd];
	vfs_cs_peer *peer = (vfs_cs_peer *) curcon->user;
	peer->hbtime = time(NULL);
	list_move_tail(&(peer->alist), &activelist);
	return SEND_CLOSE;
}

void svc_timeout()
{
	if (self_ipinfo.role == ROLE_CS)
	{
		time_t now = time(NULL);
		int to = g_config.timeout * 10;
		vfs_cs_peer *peer = NULL;
		list_head_t *l;
		list_for_each_entry_safe_l(peer, l, &activelist, alist)
		{
			if (peer == NULL)
				continue;   /*bugs */
			if (now - peer->hbtime < to)
				break;
			do_close(peer->fd);
		}
	}
}

void svc_finiconn(int fd)
{
	LOG(vfs_sig_log, LOG_DEBUG, "close %d\n", fd);
	struct conn *curcon = &acon[fd];
	if (curcon->user == NULL)
		return;
	vfs_cs_peer *peer = (vfs_cs_peer *) curcon->user;
	if (peer->local_in_fd > 0)
		close(peer->local_in_fd);
	peer->local_in_fd = -1;
	list_del_init(&(peer->alist));
	list_del_init(&(peer->hlist));
	t_vfs_tasklist *tasklist = NULL;
	if (peer->recvtask)
	{
		tasklist = peer->recvtask;
		LOG(vfs_sig_log, LOG_ERROR, "error %s!\n", tasklist->task.base.tmpfile);
		tasklist->task.base.overstatus = OVER_PEERERR;
		vfs_set_task(tasklist, TASK_FIN);
	}
	memset(curcon->user, 0, sizeof(vfs_cs_peer));
	free(curcon->user);
	curcon->user = NULL;
}
