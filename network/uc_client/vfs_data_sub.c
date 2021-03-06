/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

extern char hostname[128];
#include "c_api.h"
#include <libgen.h>
static int insert_sub_task(t_task_base *base0, int idx, int count, off_t start, off_t end)
{
	t_vfs_tasklist *task = NULL;
	int ret = vfs_get_task(&task, TASK_HOME);
	if (ret != GET_TASK_OK)
	{
		LOG(vfs_sig_log, LOG_ERROR, "vfs_get_task err %m %s\n", base0->filename);
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

	LOG(vfs_sig_log, LOG_NORMAL, "%s %s:%s:%s %ld:%ld:%d:%d\n", __FUNCTION__, base->srcip, base->filename, base->filemd5, sub->start, sub->end, sub->idx, sub->count);
	vfs_set_task(task, TASK_WAIT_SYNC);
	return 0;
}

static void update_sync_time(char *filename)
{
	struct stat filestat;
	if (stat(filename, &filestat))
	{
		LOG(vfs_sig_log, LOG_ERROR, "stat %s error %m\n", filename);
		return ;
	}

	char sync_time_file[256] = {0x0};
	snprintf(sync_time_file, sizeof(sync_time_file), "%s/.sync_time", dirname(filename));

	FILE *fp = fopen(sync_time_file, "w");
	if (!fp)
	{
		LOG(vfs_sig_log, LOG_ERROR, "open %s error %m\n", sync_time_file);
		return ;
	}

	fprintf(fp, "%ld", filestat.st_ctime); 

	fclose(fp);
}

static int split_task(t_task_base *base)
{
	if (base->fsize == 0)
		return 0;

	char filename[256] = {0x0};
	snprintf(filename, sizeof(filename), "%s", base->filename);
	update_sync_time(filename);

	int splic_count = base->fsize / g_config.splic_min_size;
	if (base->fsize % g_config.splic_min_size)
		splic_count++;

	snprintf(base->hostname, sizeof(base->hostname), "%s", hostname);
	int idx = 1;
	off_t start = 0;
	off_t end = 0;
	for(; idx <= splic_count; idx++)
	{
		if (idx == splic_count)
			end = base->fsize - 1;
		else
			end = start + g_config.splic_min_size - 1;

		if (insert_sub_task(base, idx, splic_count, start, end))
			return -1;
		start += g_config.splic_min_size;
	}
	return 0;
}

int do_req(int fd, off_t fsize)
{
	return do_prepare_recvfile(fd, fsize);
}
