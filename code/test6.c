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

	page3[0] = '\0';//tira o ZERO e aloca OK
printf("%s\n", page1);
	page0[0] = '\0';//tira o UM e alocaOK
	
	page1[0] = 'o';//Loop here
	strcat(page0, "hello");
printf("%s\n", page0);

	strcat(page3, "hell3");
printf("%s\n", page3);
	strcat(page1, "hell1");
printf("%s\n", page1);

	strcat(page2, "hell2");
printf("%s\n", page0);
	
	strcat(page3, "hello3");
printf("%s\n", page2);
	
	
//uvm_syslog(page0, strlen(page0)+1);
	exit(EXIT_SUCCESS);
}
