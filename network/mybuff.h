/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef _MYBUFF_H_
#define _MYBUFF_H_
#include <stdint.h>
#include "global.h"
#include "vfs_init.h"


//��ʼ��������С
extern int init_buff_size;
/*
 * ��ʼ��
 * mybuff		����ʼ����buff
 */
extern void mybuff_init(struct mybuff* mybuff);
/*
 * д������
 * mybuff		Ŀ��buff
 * data			����ָ��
 * len			���ݳ���
 * return		0-�ɹ�������ʧ��
 */
extern int mybuff_setdata(struct mybuff* mybuff, const char* data, size_t len);
/* 
 * д���ļ���Ϣ
 * mybuff		Ŀ��buff
 * fd			�ļ�fd
 * offset		ƫ����
 * len			���ͳ���
 * return		0-�ɹ�������ʧ��
 */
extern int mybuff_setfile(struct mybuff* mybuff, int fd, off_t offset, size_t len);
/*
 * ȡ����
 * mybuff		Դbuff
 * data			������ָ��
 * len			���ݳ���ָ��
 * return		0-�ɹ�������û������
 */
extern int mybuff_getdata(struct mybuff* mybuff, char** data, size_t* len);
/*
 * ����ʹ��������
 * mybuff		Դbuff
 * len			ʹ�ó���
 */
extern void mybuff_skipdata(struct mybuff* mybuff, size_t en);
/*
 * ȡ�ļ���Ϣ
 * mybuff		Դbuff
 * fd			�ļ�fd
 * offset		ƫ����
 * len			���ͳ���
 * return		0-�ɹ�������û������
 */
extern int mybuff_getfile(struct mybuff* mybuff, int* fd, off_t* offset, size_t * len);
/*
 * ����ʹ���ļ�������
 * mybuff		Դbuff
 * len			���ݳ���
 */
extern void mybuff_skipfile(struct mybuff* mybuff, size_t len);
/*
 * ���³�ʼ��
 * mybuff		Ŀ��buff
 */
extern void mybuff_reinit(struct mybuff* mybuff);
/*
 * �ͷ���Դ
 * mybuff		Ŀ��buff
 */
extern void mybuff_fini(struct mybuff* mybuff);

#endif
