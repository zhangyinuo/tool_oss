/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include <sys/inotify.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/prctl.h>

#include "vfs_file_filter.h"

#define MAX_WATCH 10240
char watch_dirs[MAX_WATCH][256];
int inotify_fd = -1;
fd_set fds;
uint32_t mask = IN_CLOSE_WRITE|IN_DELETE|IN_MOVED_FROM|IN_MOVED_TO|IN_CREATE;

static int add_watch(int fd, char *indir, unsigned int mask)
{
	int wd = inotify_add_watch(fd, indir, mask);
	if (wd == -1)
	{
		LOG(vfs_sig_log, LOG_ERROR, "add %s to inotify watch err %m\n", indir);
		return -1;
	}
	if (wd >= MAX_WATCH)
	{
		LOG(vfs_sig_log, LOG_ERROR, "too many wd %d add to inotify!\n", wd);
		return -1;
	}
	snprintf(watch_dirs[wd], sizeof(watch_dirs[wd]), "%s", indir);
	DIR *dp;
	struct dirent *dirp;
	if ((dp = opendir(indir)) == NULL) 
	{
		LOG(vfs_sig_log, LOG_ERROR, "add inotify watch opendir %s err %m\n", indir);
		return -1;
	}

	char file[256] = {0x0};
	struct stat filestat;
	while((dirp = readdir(dp)) != NULL)
	{
		if (dirp->d_name[0] == '.')
			continue;
		snprintf(file, sizeof(file), "%s/%s", indir, dirp->d_name);
		if (stat(file, &filestat))
		{
			LOG(vfs_sig_log, LOG_ERROR, "add inotify watch get file stat %s err %m\n", file);
			continue;
		}

		if (S_ISDIR(filestat.st_mode))
		{
			if (add_watch(fd, file, mask))
				return -1;
		}
	}
	closedir(dp);
	LOG(vfs_sig_log, LOG_TRACE, "add inotify watch %s ok\n", indir);
	return 0;
}  

static void inotify_event_handler(struct inotify_event *event)  
{
	int wd = event->wd;
	char *filename = event->name;

	if(filename == NULL )
	{
		LOG(vfs_sig_log, LOG_DEBUG, "inotify event has no filename, go next!\n");
		return;
	}
	LOG(vfs_sig_log, LOG_DEBUG, "inotify event filename [%s]!\n", filename);

	if(filename[0] == '.')
	{
		LOG(vfs_sig_log, LOG_TRACE, "tmpfile [%s] , i dont care!\n", filename);
		return;
	}

	char path[256] = {0x0};
	strcpy(path, watch_dirs[wd]);
	strcat(path, "/");
	strcat(path, filename);
	if (event->mask & IN_CREATE)
	{
		struct stat filestat;
		if (stat(path, &filestat))
		{
			LOG(vfs_sig_log, LOG_ERROR, "get file stat %s err %m\n", path);
			return;
		}

		if (S_ISDIR(filestat.st_mode))
		{
			LOG(vfs_sig_log, LOG_NORMAL, "new dir %s\n", path);
			add_watch(inotify_fd, path, mask);
		}
		else
			LOG(vfs_sig_log, LOG_NORMAL, "file %s be create, ignore it\n", path);
		return;
	}

    t_task_base task;
	memset(&task, 0, sizeof(task));

	snprintf(task.filename, sizeof(task.filename), "%s", path);
	if((event->mask & IN_CLOSE_WRITE) || (event->mask & IN_MOVED_TO)) 
	{
		if (get_localfile_stat(&task) != LOCALFILE_OK)
		{
			LOG(vfs_sig_log, LOG_ERROR, "get_localfile_stat err %s %m\n", task.filename);
			return;
		}
		task.type = TASK_ADDFILE;
    }
	
	if((event->mask & IN_DELETE) || (event->mask & IN_MOVED_FROM)) 
	{
		LOG(vfs_sig_log, LOG_DEBUG, "inotify file delete or move away %s\n", path);
		task.type = TASK_DELFILE;
	}

	split_task(&task);
 
	/*
	t_vfs_tasklist *vfs_task;
	int ret = vfs_get_task(&vfs_task, TASK_HOME);
	if(ret != GET_TASK_OK) 
	{
		LOG(vfs_sig_log, LOG_ERROR, "inotify get task_home error %s:%d:%d\n", ID, FUNC, LN);
		return;
	}
	memset(&(vfs_task->task), 0, sizeof(t_vfs_taskinfo));
	memcpy(&(vfs_task->task.base), &task, sizeof(task));
	vfs_set_task(vfs_task, TASK_WAIT);	
	*/
	
	LOG(vfs_sig_log, LOG_DEBUG, "inotify add task to task_wait filepath %s, task type %d\n", task.filename, task.type);
	return;
}   

static void start_inotify_thread(void * arg)
{

#ifndef PR_SET_NAME
#define PR_SET_NAME 15
#endif
	memset(watch_dirs, 0, sizeof(watch_dirs));
	prctl(PR_SET_NAME, "fcs_inotify", 0, 0, 0);
	pthread_detach(pthread_self());

	inotify_fd = inotify_init();
	if(inotify_fd < 0)
	{
		stop = 1;
		LOG(vfs_sig_log, LOG_ERROR, "inotify thread init error!\n");
		return ;
	}

	char *flvdir = myconfig_get_value("vfs_src_datadir");
	if(!flvdir)
	{
		flvdir = "/flvdata";
	}

	FD_ZERO(&fds);
	FD_SET(inotify_fd, &fds);
	int ret = add_watch(inotify_fd, flvdir, mask);
	if(ret != 0)
	{
		stop = 1;
		LOG(vfs_sig_log, LOG_ERROR, "inotify add watch dir error!\n");
		return ;
	}

	LOG(vfs_sig_log, LOG_DEBUG, "start to inotify watch dir\n");

	struct timeval tv;
	//循环检查事件
	while(1) 
	{
		tv.tv_sec = 20;
		tv.tv_usec = 0;
		if (select(inotify_fd + 1, &fds, NULL, NULL, &tv) > 0)
		{
			while (1)
			{
				int len, index = 0;
				unsigned char buf[1024] = {0};
				len = read(inotify_fd, &buf, sizeof(buf));
				if (len <= 0)
					break;
				while (index < len)
				{
					struct inotify_event *event = (struct inotify_event *)(buf + index);
					inotify_event_handler(event);
					index += sizeof(struct inotify_event) + event->len;
				}
			}
		}
		FD_ZERO(&fds);
		FD_SET(inotify_fd, &fds);
	}
}
