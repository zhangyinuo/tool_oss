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

static inline int isDigit(const char *ptr) 
{
	return isdigit(*(unsigned char *)ptr);
}

static int active_connect(char *ip, int port)
{
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

static void create_header(char *httpheader, t_task_base *base, t_task_sub *sub, off_t start)
{
	strcat(httpheader, "GET /us_oss HTTP/1.1\r\n");

	strcat(httpheader, "filename: ");
	strcat(httpheader, base->filename);
	strcat(httpheader, "\r\n");

	strcat(httpheader, "type: 2\r\n");
	char sbuf[64] = {0x0};
	snprintf(sbuf, sizeof(sbuf), "datalen: %ld\r\n", sub->end - sub->start + 1);
	strcat(httpheader, sbuf);

	memset(sbuf, 0, sizeof(sbuf));
	snprintf(sbuf, sizeof(sbuf), "start: %ld\r\n", start);
	strcat(httpheader, sbuf);

	memset(sbuf, 0, sizeof(sbuf));
	snprintf(sbuf, sizeof(sbuf), "end: %ld\r\n", sub->end);
	strcat(httpheader, sbuf);

	memset(sbuf, 0, sizeof(sbuf));
	snprintf(sbuf, sizeof(sbuf), "filemd5: %s\r\n\r\n", base->filemd5);
	strcat(httpheader, sbuf);
}

static int get_task_unok(char *buf, int buflen, uint32_t srcip)
{
	int retlen = 0;
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
		vfs_set_task(task, TASK_WAIT);
	}

	uint16_t once_run = 0;
	while (1)
	{
		once_run++;
		if (once_run >= g_config.cs_max_task_run_once)
			break;
		ret = vfs_get_task(&task, TASK_WAIT);
		if (ret != GET_TASK_OK)
			break;
		t_task_base *base = &(task->task.base);
		if (base->usrcip != srcip)
		{
			vfs_set_task(task, TASK_WAIT_TMP);
			continue;
		}
		t_task_sub *sub = &(task->task.sub);
		char subbuf[1024] = {0x0};
		int sublen = snprintf(subbuf, sizeof(subbuf), "%s %ld %ld %d %d\n", base->filename, sub->start, sub->end, sub->idx, sub->count);
		if (sublen + retlen >= buflen)
		{
			vfs_set_task(task, TASK_WAIT_TMP);
			break;
		}
		retlen += snprintf(buf + retlen, buflen - retlen, "%s", subbuf);
		vfs_set_task(task, TASK_HOME);
	}

	return retlen;
}


