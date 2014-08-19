/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef _DAEMON_H_
#define _DAEMON_H_

volatile extern int stop;		//1-������ֹͣ��0-������������
volatile extern int restart;	//1-�������쳣�˳���Ҫ�Զ�������0-���Զ�����
extern int daemon_start(int, char **);
extern void daemon_stop();
extern void daemon_set_title(const char *title);
#endif
