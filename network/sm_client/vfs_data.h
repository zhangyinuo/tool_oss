/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef __VFS_SIG_SO_H
#define __VFS_SIG_SO_H
#include "list.h"
#include "global.h"
#include "vfs_init.h"
#include "vfs_task.h"
#include "common.h"
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <dirent.h>

#define SELF_ROLE ROLE_CS

enum SOCK_STAT {LOGOUT = 0, CONNECTED, RECV_HEAD_ING, RECV_BODY_ING};

extern const char *sock_stat_cmd[] ;

typedef struct {
	list_head_t alist;
	list_head_t hlist;
	t_uc_oss_http_header header;
	int fd;
	int local_in_fd; /* 当cs接受对端文件传输时，打开的本地句柄 该fd由插件自己管理 */
	uint32_t hbtime;
	uint32_t ip;
	uint32_t headlen; /*当前传输文件的头信息长度*/
	uint8_t sock_stat;   /* SOCK_STAT */
	t_vfs_tasklist *recvtask; /*当前数据链路正在执行的数据接收任务 */
} vfs_cs_peer;

typedef struct {
	char username[64];
	char password[64];
	char host[32];
	int port;
} t_vfs_up_proxy;

#endif
