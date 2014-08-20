static int do_merge_file(char *filename, int count)
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
		vfs_set_task(task, TASK_HOME);
		close(ifd);
		if (error)
			break;
	}
	close(ofd);
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
		if (OVER_OK == task->task.base.overstatus)
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
				do_merge_file(task->task.base.filename, task->task.sub.count);
		}
	}
}
