/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

/*
 *���ļ�ʵ��vfsϵͳ������ɫ����ʱ����״̬����
 *ÿ����¼�������ֶ���ɣ�����״̬ '0' ������� '1' ����ΪGET�ļ� '2' ����ΪPUSH�ļ� ���� 1 
 * �ո���Ϊ�ָ���
 */

#ifndef _VFS_TMP_STATUS_H_
#define _VFS_TMP_STATUS_H_

#include "vfs_task.h"
#include "vfs_localfile.h"

typedef struct {
	off_t pos;
	list_head_t list;
} t_tmp_status;

#define DEFAULT_ITEMS 1024

/*
 * ��������ʱ װ����ʱ״̬�ļ����ڴ棬����������֮ǰ����鱾���Ƿ��Ѿ��ж�Ӧ��Ŀ���ļ�����ʱ�ļ����ϵ�������
 *
 */
int init_load_tmp_status();

/*
 *������ ����ʱ�ļ���ȡһ����λ
 *���ض�Ӧ�Ŀ�λλ�� �����Ϣ�Ѿ���λ
 */
int set_task_to_tmp(t_vfs_tasklist *tasklist);


/*
 *�ÿն�Ӧ��λ����Ϣ
 */
void set_tmp_blank(off_t pos, t_tmp_status *tmp);

#endif
