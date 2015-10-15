#include <stdio.h>
#include <stdlib.h>
#include "pager.h"

void pager_init(int nframes, int nblocks){
	printf("pager_init");
}


void pager_create(pid_t pid){
	printf("pager_create");
}


void *pager_extend(pid_t pid){
	printf("pager_extend");
}


void pager_fault(pid_t pid, void *addr){
	printf("pager_fault");
}


int pager_syslog(pid_t pid, void *addr, size_t len){
	printf("pager_syslog");
	return 0;
}


void pager_destroy(pid_t pid){
	printf("pager_destroy");
}