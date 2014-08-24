/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "c_api.h"
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

static int split_task(t_task_base *base)
{
	if (base->fsize == 0)
		return 0;

	int splic_count = base->fsize / g_config.splic_min_size;
	if (splic_count == 0)
		splic_count = 1;

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
