#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "moni.h"
#include "myconfig.h"
#include "task.h"
#include "mytime.h"
#include "util.h"
#include "log.h"

void check_tc() 
{
	if(tc_pending < 0)
		return;

	char buf[2048] = {0};
	char* field[16];
	FILE* file = fopen("/proc/net/dev", "r");
	if(file) {
		fread(buf, 1, sizeof(buf) - 1, file);
		fclose(file);
		char* p = strstr(buf, tc_if);
		if(!p) {
			DLOG(LOG_FAULT, "get tc_str fail, buf=%s, tc_if=%s\n", buf, tc_if);
			return;	
		}
		p += strlen(tc_if) + 1;// example: "eth0:"
		char* q = strstr(p, "\n");
		if(!q) {
			DLOG(LOG_FAULT, "get tc_str fail, buf=%s, tc_if=%s\n", buf, tc_if);
			return;
		}
		*q = '\0';

		int n = str_explode(NULL, p, field, 16);	
		if(n < 16) {
			DLOG(LOG_FAULT, "get tc_size fail, tc_str=%s, n=%d\n", p, n);
			tc_pending = 0;
			return;		
		}
	}
	else {
		DLOG(LOG_FAULT, "get tc_size fail, fopen fail, %m\n");
		return;
	}

	curr_tc_size = (unsigned long long)atoll(field[tc_idx]);	

	if(last_tc_size > 0) {
		unsigned sec = gnow - last_tc_time;
		unsigned long long bytes;

		if(!is64bit)
			bytes = (curr_tc_size - last_tc_size + (unsigned)-1) % (unsigned)-1;		
		else
			bytes = curr_tc_size - last_tc_size;

		tc_curr_tc = bytes / sec;
		if(tc_curr_tc < tc_max_tc) {

			if(tc_max_tc <= tc_curr_tc * 1.2)
				tc_pending -= tc_pending_delta;
			else
				tc_pending -= (tc_pending_delta<<2);	

			if(tc_pending < 0)
				tc_pending = 0;

		}
		else {

			if(tc_max_tc * 1.2 <= tc_curr_tc)
				tc_pending += (tc_pending_delta<<2);
			else
				tc_pending += tc_pending_delta;

			if(tc_pending > tc_pending_max)
				tc_pending = tc_pending_max;

		}
		//求通过率
		if(tc_pending == 0)
			tc_pass_rate = 1.0;
		else		
			tc_pass_rate = (double)(tc_pending_max - tc_pending) / (double)tc_pending_max;	

		DLOG(LOG_NORMAL, "TRAFFIC_CONTROL %s %s tc_now=%d/%d tc_pending=%d/%d\n", tc_if, tc_idx == 0 ? "rx" : "tx", tc_curr_tc, tc_max_tc, tc_pending, tc_pending_max);
	}

	last_tc_size = curr_tc_size;
	last_tc_time = gnow;

}

int init_tc() 
{

	int n = reload_tc();

	if(n == 1) {
		srand(gnow);
		register_timer_task_i(check_tc, NULL, 1);
		myconfig_register_reload_i(reload_tc, NULL, 0, 1);
	}

	if(n < 0)
		return n;
	else
		return 0;
}
