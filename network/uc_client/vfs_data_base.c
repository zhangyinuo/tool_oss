/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

/*
 *base�ļ�����ѯ����������Ϣ��������ػ���״̬������```
 *Tracker ��Ŀ���٣�����һ����̬����
 *CS��FCS��Ŀ�϶࣬����hash����
 *CS FCS ip��Ϣ����uint32_t �洢�����ڴ洢�Ͳ���
 */
volatile extern int maintain ;		//1-ά������ 0 -����ʹ��
extern t_vfs_up_proxy g_proxy;

static int active_send(int fd, char *data)
{
	LOG(vfs_sig_log, LOG_DEBUG, "send %d cmdid %s\n", fd, data);
	set_client_data(fd, data, strlen(data));
	modify_fd_event(fd, EPOLLOUT);
	return 0;
}

static int active_connect()
{
	char *ip = myconfig_get_value("iplist_serverip");
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

static void create_header(char *httpheader, t_task_base *base)
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
	snprintf(sbuf, sizeof(sbuf), "start: 0\r\n");
	strcat(httpheader, sbuf);

	memset(sbuf, 0, sizeof(sbuf));
	snprintf(sbuf, sizeof(sbuf), "end: %ld\r\n", base->fsize - 1);
	strcat(httpheader, sbuf);

	memset(sbuf, 0, sizeof(sbuf));
	snprintf(sbuf, sizeof(sbuf), "filemd5: %s\r\n", base->filemd5);
	strcat(httpheader, sbuf);

	memset(sbuf, 0, sizeof(sbuf));
	snprintf(sbuf, sizeof(sbuf), "filectime: %ld\r\n\r\n", base->file_ctime);
	strcat(httpheader, sbuf);
}

static int inotify_new_task(t_task_base *base)
{
	char httpheader[1024] = {0x0};

	create_header(httpheader, base);

	int fd = active_connect();
	if (fd < 0)
	{
		LOG(vfs_sig_log, LOG_ERROR, "connect err %m\n");
		return -1;
	}

	return active_send(fd, httpheader);
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
		vfs_set_task(task, TASK_WAIT);
	}

	uint16_t once_run = 0;
	while (1)
	{
		once_run++;
		if (once_run >= g_config.cs_max_task_run_once)
			return;
		ret = vfs_get_task(&task, TASK_WAIT);
		if (ret != GET_TASK_OK)
			return ;

		t_task_base *base = &(task->task.base);
		if (inotify_new_task(base))
			vfs_set_task(task, TASK_WAIT_TMP);
		else
			vfs_set_task(task, TASK_HOME);
	}
}


