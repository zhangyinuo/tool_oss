/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "vfs_localfile.h"
#include "vfs_time_stamp.h"
#include "vfs_file_filter.h"
#include "vfs_timer.h"
#include "common.h"
#include "util.h"
#include "vfs_del_file.h"
#include <sys/vfs.h>
#include <utime.h>
#include <libgen.h>
extern int glogfd;
extern t_g_config g_config;
extern t_ip_info self_ipinfo;

/*
 *本文件有优化余地
 */

static int createdir(char *file)
{
	char *pos = file;
	int len = 1;
	while (1)
	{
		pos = strchr(file + len, '/');
		if (!pos)
			break;
		*pos = 0;
		if (access(file, F_OK) != 0)
		{
			if (mkdir(file, 0755) < 0)
			{
				LOG(glogfd, LOG_ERROR, "mkdir [%s] [%m][%d]!\n", file, errno);
				return -1;
			}
			if (chown(file, g_config.dir_uid, g_config.dir_gid))
				LOG(glogfd, LOG_ERROR, "chown [%s] %d %d [%m][%d]!\n", file, g_config.dir_uid, g_config.dir_gid, errno);
		}

		*pos = '/';
		len = pos - file + 1;
	}
	if (chown(file, g_config.dir_uid, g_config.dir_gid))
		LOG(glogfd, LOG_ERROR, "chown [%s] %d %d [%m][%d]!\n", file, g_config.dir_uid, g_config.dir_gid, errno);
	return 0;
}

int get_localdir(char *srcfile, char *dstfile)
{
	char *p = srcfile + g_config.src_root_len;
	sprintf(dstfile, "%s/%s", myconfig_get_value("vfs_dst_datadir"), p);
	return 0;
}

void real_rm_file(char *file)
{
	int ret = unlink(file);
	if (ret && errno != ENOENT)
	{
		LOG(glogfd, LOG_ERROR, "file [%s] unlink err %m\n", file);
		return ;
	}
	LOG(glogfd, LOG_NORMAL, "file [%s] be unlink\n", file);
}

int delete_localfile(char *srcfile)
{
	char outfile[256] = {0x0};
	get_localdir(srcfile, outfile);
	if (unlink(outfile))
		LOG(glogfd, LOG_ERROR, "file unlink err[%s:%m]\n", outfile);

	return LOCALFILE_OK;
}

int check_localfile_md5(char *srcfile, char *md5sum)
{
	char outfile[256] = {0x0};
	get_localdir(srcfile, outfile);
	char filemd5[36] = {0x0};
	getfilemd5view((const char*)outfile, (unsigned char *)filemd5);

	return strncmp(md5sum, filemd5, 32);
}

int check_last_file(t_task_base *task, t_task_sub *sub)
{
	char outfile[256] = {0x0};
	get_localdir(task->filename, outfile);
	snprintf(task->tmpfile, sizeof(task->tmpfile), "%s_%ld_%ld", outfile, sub->start, sub->end);
	if (createdir(task->tmpfile))
	{
		LOG(glogfd, LOG_ERROR, "dir %s create %m!\n", task->tmpfile);
		return LOCALFILE_DIR_E;
	}
	struct stat filestat;
	if (stat(task->tmpfile, &filestat))
		return LOCALFILE_OK;

	off_t reqlen = sub->end - sub->start + 1;
	if (reqlen <= filestat.st_size)
	{
		task->getlen = 0;
		if (unlink(task->tmpfile))
		{
			LOG(glogfd, LOG_ERROR, "unlink %s err %m!\n", task->tmpfile);
			return LOCALFILE_DIR_E;
		}
		return LOCALFILE_OK;
	}
	task->getlen = filestat.st_size;
	return LOCALFILE_OK;
}

int open_tmp_localfile_4_write(t_task_base *task, int *fd, t_task_sub *sub)
{
	char outfile[256] = {0x0};
	get_localdir(task->filename, outfile);
	snprintf(task->tmpfile, sizeof(task->tmpfile), "%s_%ld_%ld", outfile, sub->start, sub->end);
	if (createdir(task->tmpfile))
	{
		LOG(glogfd, LOG_ERROR, "dir %s create %m!\n", task->tmpfile);
		return LOCALFILE_DIR_E;
	}
	*fd = open(task->tmpfile, O_CREAT | O_WRONLY| O_LARGEFILE |O_APPEND, 0644);
	//*fd = open(task->tmpfile, O_CREAT | O_WRONLY| O_LARGEFILE |O_TRUNC, 0644);
	if (*fd < 0)
	{
		LOG(glogfd, LOG_ERROR, "open %s err %m\n", task->tmpfile);
		return LOCALFILE_OPEN_E;
	}
	return LOCALFILE_OK;
}

int close_tmp_check_mv(t_task_base *task, int fd)
{
	if (fd < 0)
	{
		LOG(glogfd, LOG_ERROR, "fd < 0 %d %s\n", fd, task->filename);
		return LOCALFILE_OPEN_E;
	}
	close(fd);
	return LOCALFILE_OK;
}

void localfile_link_task(t_task_base *task)
{
}

int get_localfile_stat(t_task_base *task)
{
	struct stat filestat;
	if (stat(task->filename, &filestat))
	{
		LOG(glogfd, LOG_ERROR, "stat file %s err %m!\n", task->filename);
		return LOCALFILE_DIR_E;
	}
	task->fsize = filestat.st_size;
	task->file_ctime = filestat.st_ctime;
	getfilemd5view((const char*)task->filename, (unsigned char *)task->filemd5);
	return LOCALFILE_OK;
}

