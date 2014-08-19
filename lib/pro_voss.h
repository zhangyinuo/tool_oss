/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef __PROTOCOL_H_
#define __PROTOCOL_H_

#define VOSSPREFIX "voss_"
#define VOSSSUFFIX ".master"

#define CMD_MASK 0x80000000

#define REQ_AUTH 0x00000001 //������Ȩ����
#define RSP_AUTH 0x80000001 //������ȨӦ��

#define REQ_SUBMIT 0x00000002 //�ύĩ�˻���Ϣ����
#define RSP_SUBMIT 0x80000002 //�ύĩ�˻���ϢӦ��

#define REQ_CONF_UPDATE 0x00000003   //����vfs����������
#define RSP_CONF_UPDATE 0x80000003   //����VFS������Ӧ��

#define REQ_SYNC_DIR 0x00000004   //FCS��������VOSS��Ŀ¼ͬ������ ������Ϊ�첽����
#define RSP_SYNC_DIR 0x80000004   //FCS��������VOSS��Ŀ¼ͬ��Ӧ��

#define REQ_SYNC_FILE 0x00000006 //TRACKER ��������VOSS�� С��Ӫ���ļ�ͬ������
#define RSP_SYNC_FILE 0x80000006 //TRACKER ��������VOSS�� С��Ӫ���ļ�ͬ��Ӧ��

#define REQ_HEARTBEAT 0x00000005
#define RSP_HEARTBEAT 0x80000005

#define REQ_VFS_CMD 0x00000008  //VFS��������VOSS����
#define RSP_VFS_CMD 0x80000008  //VFSӦ���VOSS�ı���

#define REQ_STOPVFS 0x00000009 //VFS��������VOSS��ֹͣ����
#define RSP_STOPVFS 0x80000009

#define HEADSIZE 12
#define MAXBODYSIZE 20480

enum LINKSTAT {CONNETED = 0, LOGON, RUN, START, EXE_TASK, END, LINKSEND, LINKTEST};

enum RETCODE {OK = 0, E_NOT_SUFFIC, E_TOO_LONG};  /*too long need close , reset socket*/

typedef struct {
	char buf[MAXBODYSIZE];
}t_body_info;

typedef struct {
	unsigned int totallen;
	unsigned int cmdid;
	unsigned int seq;
}t_head_info;

/* create_msg:����������Ϣ
 *outbuf:�������������Ź���õ���Ϣ
 *outlen:�������������
 *cmdid:������
 *inbuf:body�Ļ�����
 *inlen:body�ĳ���
 *����ֵ0:OK  -1:ERROR
 */

int create_msg(char *outbuf, int *outlen, unsigned int cmdid, char *inbuf, int inlen);

/*parse_msg:������Ϣ
 *inbuf:���뻺����
 *inlen:���뻺��������
 *head:�������ı���head
 *����ֵ0:OK  other:ERROR
 */

int parse_msg(char *inbuf, int inlen, t_head_info *head);

int create_voss_head(char *outbuf, unsigned int cmdid, int datalen);
#endif
