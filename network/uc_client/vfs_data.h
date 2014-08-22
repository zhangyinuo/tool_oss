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
	int fd;
	int local_in_fd; /* ��cs���ܶԶ��ļ�����ʱ���򿪵ı��ؾ�� ��fd�ɲ���Լ����� */
	uint32_t hbtime;
	uint32_t ip;
	uint32_t headlen; /*��ǰ�����ļ���ͷ��Ϣ����*/
	uint8_t sock_stat;   /* SOCK_STAT */
	t_vfs_tasklist *recvtask; /*��ǰ������·����ִ�е����ݽ������� */
} vfs_cs_peer;

typedef struct {
	char username[64];
	char password[64];
	char host[32];
	int port;
} t_vfs_up_proxy;

#endif
