#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "uvm.h"

int main(void) {
	uvm_create();
	char *page0 = uvm_extend();
	char *page1 = uvm_extend();
	char *page2 = uvm_extend();
	char *page3 = uvm_extend();
	page0[0] = '\0';
	page1[0] = '\0';
	page2[0] = '\0';

	page3[0] = '\0'; //Aqui vai mandar page0 p/ o disco

	strcat(page1,"Hi");

	strcat(page0, "World!");
	
	printf("%s %s\n",page1, page0);
	uvm_syslog(page1, strlen(page1)+1);
	uvm_syslog(page0, strlen(page0)+1);
	exit(EXIT_SUCCESS);
}
