#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pager.h"
#include "uvm.h"
#include "mmu.h"

int main(void) {
	uvm_create();
	char *p = uvm_extend();
	p[0]='+';
	strcat(p,"kk");

	uvm_syslog(p, 1);
        //Deveria imprimir:
	//     oieZERO
	//	UMMMM

	//Mas dá erro em uvm.c
	exit(EXIT_SUCCESS);
}


