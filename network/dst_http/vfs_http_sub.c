int active_send(int fd, char *data)
{
	LOG(vfs_http_log, LOG_DEBUG, "send %d cmdid %s\n", fd, data);
	set_client_data(fd, data, strlen(data));
	modify_fd_event(fd, EPOLLOUT);
	return 0;
}

static int active_connect(char *ip, int port)
{
	if (port < 80)
		port = 80;
	int fd = createsocket(ip, port);
	if (fd < 0)
	{
		LOG(vfs_http_log, LOG_ERROR, "connect %s:%d err %m\n", ip, port);
		return -1;
	}
	if (svc_initconn(fd))
	{
		LOG(vfs_http_log, LOG_ERROR, "svc_initconn err %m\n");
		close(fd);
		return -1;
	}
	add_fd_2_efd(fd);
	LOG(vfs_http_log, LOG_NORMAL, "fd [%d] connect %s:%d\n", fd, ip, port);
	return fd;
}

static void create_header(char *httpheader, char *filename, char *filemd5, int type)
{
	strcat(httpheader, "GET /us_oss HTTP/1.1\r\n");

	strcat(httpheader, "filename: ");
	strcat(httpheader, filename);
	strcat(httpheader, "\r\n");

	char sbuf[64] = {0x0};
	snprintf(sbuf, sizeof(sbuf), "type: %d\r\n", type);
	strcat(httpheader, sbuf);
	strcat(httpheader, "datalen: 2\r\n");

	memset(sbuf, 0, sizeof(sbuf));
	snprintf(sbuf, sizeof(sbuf), "start: 0\r\n");
	strcat(httpheader, sbuf);

	memset(sbuf, 0, sizeof(sbuf));
	snprintf(sbuf, sizeof(sbuf), "end: 0\r\n");
	strcat(httpheader, sbuf);

	memset(sbuf, 0, sizeof(sbuf));
	snprintf(sbuf, sizeof(sbuf), "filemd5: %s\r\n\r\n", filemd5);
	strcat(httpheader, sbuf);
}

static int do_merge_file(char *srcip, char *filename, int count, char *filemd5)
{
	int ofd = open(filename, O_CREAT | O_WRONLY| O_LARGEFILE |O_TRUNC, 0644);
	if (ofd < 0)
	{
		LOG(vfs_http_log, LOG_ERROR, "open %s err %m\n", filename);
		return-1;
	}
	t_vfs_tasklist *task = NULL;
	int idx = 1;
	for (; idx <= count; idx++)
	{
		if (get_task_from_alltask(&task, filename, idx, count))
		{
			LOG(vfs_http_log, LOG_ERROR, "get %s %d %d err %m\n", filename, idx, count);
			break;
		}

		int ifd = open(task->task.base.tmpfile, O_RDONLY);
		if (ifd < 0)
		{
			LOG(vfs_http_log, LOG_ERROR, "open %s err %m\n", task->task.base.tmpfile);
			break;
		}

		int error = 0;
		char buf[1024000];
		struct stat st;
		fstat(ifd, &st);
		size_t retlen = st.st_size;
		while (retlen > 0)
		{
			int len = read(ifd, buf, sizeof(buf));
			if (len < 0)
			{
				LOG(vfs_http_log, LOG_ERROR, "read %s err %m\n", task->task.base.tmpfile);
				error = -1;
				break;
			}
			if (write(ofd, buf, len) != len)
			{
				LOG(vfs_http_log, LOG_ERROR, "write %s err %m\n", filename);
				error = -1;
				break;
			}
			retlen -= len;
		}
		close(ifd);
		if (unlink(task->task.base.tmpfile))
		{
			LOG(vfs_http_log, LOG_ERROR, "unlink %s err %m\n", task->task.base.tmpfile);
			error = -1;
		}
		vfs_set_task(task, TASK_HOME);
		if (error)
			break;
	}
	close(ofd);

	int type = 5;
	char md5view[36] = {0x0};
	getfilemd5view((const char *)filename, (unsigned char* )md5view);
	if (strcmp(md5view, filemd5))
	{
		LOG(vfs_http_log, LOG_ERROR, "%s md5 [%s:%s]\n", filename, filemd5, md5view);
		type = 6;
	}
	char httpheader[1024] = {0x0};
	create_header(httpheader, filename, filemd5, type);

	int fd = active_connect(srcip, 80);
	if (fd < 0)
		LOG(vfs_http_log, LOG_ERROR, "active_connect %s:80 err %m\n", srcip);
	else
		active_send(fd, httpheader);
	return 0;
}

static int check_task_allsub_isok(char *filename, int count)
{
	int idx = 1;
	for (; idx <= count; idx++)
	{
		if (check_task_from_alltask(filename, idx, count))
		{
			LOG(vfs_http_log, LOG_NORMAL, "%s %d %d not ok \n", filename, idx, count);
			return -1;
		}
	}
	return 0;
}

static void clear_alltask_timeout(char *filename, int count)
{
}

static void check_fin_task()
{
	int once = 0;
	while (1)
	{
		t_vfs_tasklist *task = NULL;
		if (vfs_get_task(&task, TASK_FIN))
			return ;
		once++;
		if (once > 128)
			return;
		if (OVER_OK != task->task.base.overstatus)
		{
			if (g_config.retry && task->task.base.retry < g_config.retry)
			{
				task->task.base.retry++;
				task->task.base.overstatus = OVER_UNKNOWN;
				LOG(vfs_http_log, LOG_NORMAL, "retry[%d:%d:%s]\n", task->task.base.retry, g_config.retry, task->task.base.tmpfile);
				vfs_set_task(task, TASK_WAIT);
			}
			else
				clear_alltask_timeout(task->task.base.filename, task->task.sub.count);
		}
		else 
		{
			add_task_to_alltask(task);
			if (check_task_allsub_isok(task->task.base.filename, task->task.sub.count) == 0)
				do_merge_file(task->task.base.srcip, task->task.base.filename, task->task.sub.count, task->task.base.filemd5);
		}
	}
}
