/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

/*
 *base文件，查询基础配置信息，设置相关基础状态及其它```
 *Tracker 数目较少，放在一个静态数组
 *CS和FCS数目较多，放在hash链表
 *CS FCS ip信息采用uint32_t 存储，便于存储和查找
 */
volatile extern int maintain ;		//1-维护配置 0 -可以使用
extern t_vfs_up_proxy g_proxy;

static int active_send(int fd, char *data, t_task_sub *sub, int lfd)
{
	LOG(vfs_sig_log, LOG_DEBUG, "send %d cmdid %s\n", fd, data);
	set_client_data(fd, data, strlen(data));
	set_client_fd(fd, lfd, sub->start, sub->end - sub->start + 1);
	modify_fd_event(fd, EPOLLOUT);
	return 0;
}

static int active_connect()
{
	char *ip = myconfig_get_value("iplist_smip");
	int port = g_config.sig_port;
	int fd = createsocket(ip, port);
	if (fd < 0)
	{
		LOG(vfs_sig_log, LOG_ERROR, "connect %s:%d err %m\n", ip, port);
		return -1;
	}
	if (svc_initconn(fd))
	{
		LOG(vfs_sig_log, LOG_ERROR, "svc_initconn err %m\n");
		close(fd);
		return -1;
	}
	add_fd_2_efd(fd);
	LOG(vfs_sig_log, LOG_NORMAL, "fd [%d] connect %s:%d\n", fd, ip, port);
	return fd;
}

static void create_header(char *httpheader, t_task_base *base, t_task_sub *sub)
{
	strcat(httpheader, "GET /us_oss HTTP/1.1\r\n");

	strcat(httpheader, "filename: ");
	strcat(httpheader, base->filename);
	strcat(httpheader, "\r\n");

	char sbuf[64] = {0x0};
	snprintf(sbuf, sizeof(sbuf), "type: %d\r\n", base->type);
	strcat(httpheader, sbuf);

	memset(sbuf, 0, sizeof(sbuf));
	snprintf(sbuf, sizeof(sbuf), "datalen: %ld\r\n", base->fsize);
	strcat(httpheader, sbuf);

	memset(sbuf, 0, sizeof(sbuf));
	snprintf(sbuf, sizeof(sbuf), "Content-Length: %ld\r\n", sub->end - sub->start + 1);
	strcat(httpheader, sbuf);

	memset(sbuf, 0, sizeof(sbuf));
	snprintf(sbuf, sizeof(sbuf), "start: %ld\r\n", sub->start);
	strcat(httpheader, sbuf);

	memset(sbuf, 0, sizeof(sbuf));
	snprintf(sbuf, sizeof(sbuf), "end: %ld\r\n", sub->end);
	strcat(httpheader, sbuf);

	memset(sbuf, 0, sizeof(sbuf));
	snprintf(sbuf, sizeof(sbuf), "filemd5: %s\r\n", base->filemd5);
	strcat(httpheader, sbuf);

	memset(sbuf, 0, sizeof(sbuf));
	snprintf(sbuf, sizeof(sbuf), "filectime: %ld\r\n", base->file_ctime);
	strcat(httpheader, sbuf);

	memset(sbuf, 0, sizeof(sbuf));
	snprintf(sbuf, sizeof(sbuf), "idx: %d\r\n", sub->idx);
	strcat(httpheader, sbuf);

	memset(sbuf, 0, sizeof(sbuf));
	snprintf(sbuf, sizeof(sbuf), "count: %d\r\n\r\n", sub->count);
	strcat(httpheader, sbuf);
}

static int inotify_new_task(t_task_base *base, t_task_sub *sub)
{
	char httpheader[1024] = {0x0};

	create_header(httpheader, base, sub);

	int lfd = open(base->filename, O_RDONLY|O_LARGEFILE);
	if (lfd < 0)
	{
		LOG(vfs_sig_log, LOG_ERROR, "open %s err %m\n", base->filename);
		return -1;
	}

	int fd = active_connect();
	if (fd < 0)
	{
		LOG(vfs_sig_log, LOG_ERROR, "connect err %m\n");
		return -1;
	}

	return active_send(fd, httpheader, sub, lfd);
}

void check_task()
{
	t_vfs_tasklist *task = NULL;
	int ret = 0;
	while (1)
	{
		ret = vfs_get_task(&task, TASK_WAIT_TMP);
		if (ret != GET_TASK_OK)
		{
			LOG(vfs_sig_log, LOG_TRACE, "vfs_get_task get notihng %d\n", ret);
			break;
		}
		vfs_set_task(task, TASK_WAIT_SYNC);
	}

	uint16_t once_run = 0;
	while (1)
	{
		once_run++;
		if (once_run >= g_config.cs_max_task_run_once)
			return;
		ret = vfs_get_task(&task, TASK_WAIT_SYNC);
		if (ret != GET_TASK_OK)
			return ;

		LOG(vfs_sig_log, LOG_DEBUG, "%s %s %d\n", ID, FUNC, LN);
		t_task_base *base = &(task->task.base);
		t_task_sub *sub = &(task->task.sub);
		if (inotify_new_task(base, sub))
			vfs_set_task(task, TASK_WAIT_TMP);
		else
			vfs_set_task(task, TASK_HOME);
	}
}


