#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pager.h"
#include "uvm.h"
#include "mmu.h"

int main(void) {
	uvm_create();
	char *page0 = uvm_extend();
	char *page1 = uvm_extend();
	char *page2 = uvm_extend();
	char *page3 = uvm_extend();
	page0[0] = '\0';//aloca
	page1[0] = '\0';//aloca
	page2[0] = '\0';//aloca

//uvm_syslog(page0, strlen(page0)+1);
	exit(EXIT_SUCCESS);
}
