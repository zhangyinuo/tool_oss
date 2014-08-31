#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "moni.h"
#include "myconfig.h"
#include "global.h"
#include "util.h"
#include "log.h"

static char *tc_face_name = NULL;
static int recv_limit = 0;
static int send_limit = 0;

int recv_pass_ratio;
int send_pass_ratio; 
int max_pend_value = 0xFF;

static void get_tc_recv_send(uint64_t *recv_bytes, uint64_t *send_bytes)
{
	char cmdstr[256] = {0x0};
	snprintf(cmdstr, sizeof(cmdstr), "cat /proc/net/dev |grep %s |awk -F\: '{print $2}' |awk '{print $1\" \"$9}'", tc_face_name);

	FILE *fp = fopen(cmdstr, "r");
	if (fp == NULL)
	{
		send_pass_ratio = max_pend_value;
		recv_pass_ratio = max_pend_value;
		return;
	}
	char buf[256] = {0x0};
	fread(buf, 1, sizeof(buf), fp);
	pclose(fp);

}

void check_tc() 
{
}

int init_tc() 
{
	send_pass_ratio = max_pend_value;
	recv_pass_ratio = max_pend_value;
	tc_face_name = myconfig_get_value("tc_face_name");
	if (tc_face_name == NULL)
	{
		LOG(glogfd, LOG_DEBUG, "no tc_face_name.\n");
		return 0;
	}
	LOG(glogfd, LOG_DEBUG, "tc_face_name = %s\n", tc_face_name);

	recv_limit = get_tc_limit("recv_limit");
	send_limit = get_tc_limit("send_limit");
	return 0;
}
