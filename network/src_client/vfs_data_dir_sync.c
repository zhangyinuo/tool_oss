static time_t get_sync_time(char *indir)
{
	char sync_time_file[256] = {0x0};
	snprintf(sync_time_file, sizeof(sync_time_file), "%s/.sync_time", indir);

	FILE *fp = fopen(sync_time_file, "r");
	if (!fp)
	{
		if (errno != ENOENT)
			LOG(vfs_sig_log, LOG_ERROR, "open %s error %m\n", sync_time_file);
		return 0;
	}

	char buf[16] = {0x0};
	fgets(buf, sizeof(buf), fp);
	fclose(fp);

	return atol(buf);
}

static void do_sync_dir(char *indir)
{
	DIR *dp;
	struct dirent *dirp;
	if ((dp = opendir(indir)) == NULL) 
	{
		LOG(vfs_sig_log, LOG_ERROR, "opendir %s err %m\n", indir);
		return ;
	}

	time_t last_sync_time = get_sync_time(indir);

	char file[256] = {0x0};
	struct stat filestat;
	while((dirp = readdir(dp)) != NULL)
	{
		if (dirp->d_name[0] == '.')
			continue;
		snprintf(file, sizeof(file), "%s/%s", indir, dirp->d_name);
		if (stat(file, &filestat))
		{
			LOG(vfs_sig_log, LOG_ERROR, "file stat %s err %m\n", file);
			continue;
		}

		if (S_ISDIR(filestat.st_mode))
		{
			do_sync_dir(file);
			continue;
		}

		t_task_base task;
		memset(&task, 0, sizeof(task));

		snprintf(task.filename, sizeof(task.filename), "%s", file);
		if (get_localfile_stat(&task) != LOCALFILE_OK)
		{
			LOG(vfs_sig_log, LOG_ERROR, "get_localfile_stat err %s %m\n", task.filename);
			continue;
		}
		if (task.file_ctime < last_sync_time)
		{
			LOG(vfs_sig_log, LOG_NORMAL, "file %s sync already\n", task.filename);
			continue;
		}
		task.type = TASK_ADDFILE;

		t_vfs_tasklist *vfs_task;
		int ret = vfs_get_task(&vfs_task, TASK_HOME);
		if(ret != GET_TASK_OK) 
		{
			LOG(vfs_sig_log, LOG_ERROR, "sync_dir get task_home error %s:%d:%d\n", ID, FUNC, LN);
			continue;
		}
		memset(&(vfs_task->task), 0, sizeof(t_vfs_taskinfo));
		memcpy(&(vfs_task->task.base), &task, sizeof(task));
		vfs_set_task(vfs_task, TASK_WAIT);	

		LOG(vfs_sig_log, LOG_DEBUG, "sync_dir task to task_wait filepath %s, task type %d\n", task.filename, task.type);
	}
	closedir(dp);
	LOG(vfs_sig_log, LOG_NORMAL, "do_sync_dir %s ok\n", indir);
}

static void sync_dir_thread(void *arg)
{
	pthread_detach(pthread_self());
	char *flvdir = myconfig_get_value("vfs_src_datadir");
	if(!flvdir)
		flvdir = "/flvdata";

	do_sync_dir(flvdir);
}
