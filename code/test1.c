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
	
	page0[0] = '\0';//aloca na posicao 0x60000000
	strcat(page0, "oie");//Escreve "oie" em 0x60000000

	page1[0] = '\0';//aloca na posicao 0x60001000
	strcat(page1,"UMMMM");//Escreve "UMMMM" em 0x60001000

	page2[0] = '\0';//aloca na posicao 0x60002000
	
	page3[0] = '\0';//aloca na posicao 0x60003000

	strcat(page0, "ZERO"); 
	//DEVERIA ESCREVER "ZERO" NA POSICAO 0x60000000. 
	//Mas ele tenta escrever na posição 0x60000003 . 
	//E esse endereço não foi alocado. Devo sempre usar (VAddr mod SC_PAGESIZE) ou
	//Esse erro é do UVM que não tratou um endereço não alocado?




	printf("%s\n", page0);

	printf("%s\n", page3);

	printf("%s\n", page1);

	exit(EXIT_SUCCESS);
}
