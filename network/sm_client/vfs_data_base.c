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

static void create_header(char *httpheader, char *data)
{
	//strcat(httpheader, "HTTP/1.1 200 OK\r\n");
	strcat(httpheader, "GET /question/19973178 HTTP/1.0\r\n");

	strcat(httpheader, "filename: ");
	strcat(httpheader, data);
	strcat(httpheader, "\r\n");

	strcat(httpheader, "type: 251\r\n\r\n");
}

static int get_task_unok(char *httpheader, char *hostname)
{
	char buf[10240] = {0x0};
	int buflen = sizeof(buf);
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
		if (strcmp(base->hostname, hostname))
		{
			vfs_set_task(task, TASK_WAIT_TMP);
			continue;
		}
		t_task_sub *sub = &(task->task.sub);
		char subbuf[1024] = {0x0};
		int sublen = snprintf(subbuf, sizeof(subbuf), "%s %d %d:", base->filename, sub->idx, sub->count);
		LOG(vfs_sig_log, LOG_NORMAL, "content %s\n", subbuf);
		if (sublen + retlen >= buflen)
		{
			vfs_set_task(task, TASK_WAIT_TMP);
			break;
		}
		retlen += snprintf(buf + retlen, buflen - retlen, "%s", subbuf);
		vfs_set_task(task, TASK_HOME);
	}

	create_header(httpheader, buf);
	return strlen(httpheader);
}


