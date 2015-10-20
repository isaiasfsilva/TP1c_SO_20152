#include <stdio.h>
#include <stdlib.h>
#include "pager.h"

									/*------- INÍNIO ESTRUTURA DE DADOS ------ */

//[Estrutura CELL]: Responsável em armazenar todas as informações de uma entrada
//de página do processo X.
struct Cell{
	//[VAddr]: Endereço Virtual
	int VAddr; 		

	//[RAMAddr]: Endereço Real na Memória RAM
	int RAMAddr;	

	//[Local]:0 significa que o dado está na Memória, e 1 o dado está no disco
	char Local; 	

	//[Access]: Flag de acesso. Salva a última vez que acessamos o endereço X. 
	//Será usada na política de remoção da segunda chance
	int Access;		
					
	//[Dirty]: Flag que diz se a memória está suja. Será utilizado para salvar 	
	//o dado no disco. Se tiver limpo e o dado já estiver no disco,
	//não precisa fazer nada. Caso contrário deve substituir/ colocar no disco		
	char Dirty;		
	
	//[DiskQuadroAddr]: Endereço correspondente ao quadro no disco.
	//Todo espaço na memória deve ter um espaço reservado para colocar no disco.
	int DiskQuadroAddr;

	//[AddrReady]: Flag responsável em dizer se a página na memória real está 
	//pronta para o processo usar. Quando um processo pedir mais memória, não
	//faremos nada na memória, somente bloqueamos na nossa lista para que não
	//seja alocado para outro processo. Quando o processo de fato precisar usar
	//o endereço, aí sim a gente limpa o endereço da memória.
	char AddrReady;				
	
	//[next]: Ponteiro para o próximo elemento da lista				
    struct Cell *next;
};

typedef struct Cell MemItem;

//[Estrutura PROCESS]: Esta estrutura aponta para a lista de entradas de que 
//um processo tem.
struct Process{
	//[PID]: PID do processo
	int PID;

	//[mem]: Aponta para o início da lista que contém os itens alocados para
	//este processo
	struct Cell *mem;

	//[next]: Ponteiro para o próximo processo
	struct Process *next;
};

typedef struct Process PIDItem;


											/*---------FIM ESTRUTURA DE DADOS -------*/


											/*----[FUNÇÕES DA LISTA DE PROCESSOS]----*/



//[P_isEmpty]: Verifica se a lista está vazia
//RETORNO:
//		1 se lista está vazia
//		0 se lista não está vazia.
int P_isEmpty(PIDItem *p){
	return (p->next==NULL)?1:0;
}

//[P_start]: Cria a lista de processos. Esse será a célu cabeça
void P_start(PIDItem* p){
	p->next=NULL;
}

//[P_insere]: Cria uma nova entrada para um processo.
//Ele insere na ordem dos PIDS
//RETORNO:
//		1 se deu tudo certo
//		0 se deu erro
int P_insere(PIDItem *p, int PID){
	PIDItem *pnew = (PIDItem *)malloc(sizeof(PIDItem));

	if(!pnew)	 //Se não tem memória ou deu erro
		return 0;

	//Inicializa PNEW
	pnew->next=NULL;
	pnew->mem=NULL;
	pnew->PID=PID;

	if(P_isEmpty(p))
		p->next=pnew;
	else{
		PIDItem *tmp = p;
		while(tmp->next!=NULL && tmp->next->PID<PID)
			tmp=tmp->next;
		pnew->next=tmp->next;
		tmp->next=pnew;
	}
	return 1;
}

//[P_remove]: Remove uma entrada de um processo. Ele também chama 
//o método de liberar as memórias da outra lista
//RETORNO
//		1 se removeu certinho (se existia na lista)
//		0 se deu erro
int P_remove(PIDItem *p, int PID){
	PIDItem *tmp = p;
	while(tmp->next!=NULL && tmp->next->PID!=PID)
		tmp=tmp->next;

	if(!tmp->next) //Se elemento não existe
		return 0;
	else{
		PIDItem *remover = tmp->next;
		tmp->next=tmp->next->next;
		//M_libera(remover->mem);//Remove todas as entradas da memória
		free(remover);
		return 1;
	}
}

//[P_isset]: Informa se o processo existe ou não. Se existir ele já retorna o
//endereço da lista de páginas desse processo
//RETORNO
//		*MemItem: Endereço da célula cabeça dos espaçoes de memória
//		desse processo
//		
//		NULL: Se o processo não existe
MemItem *P_isset(PIDItem *p, int PID){
	PIDItem *tmp = p;
	while(tmp->next!=NULL && tmp->next->PID!=PID)
		tmp=tmp->next;
	return tmp->next->mem;
}
											/*--------- FIM FUNÇÕES LISTA DE PROCESSOS----------*/




											/*---- FUNÇÕES DA LISTA DE PÁGINAS DO PROCESSO ---- */




//[M_isEmpty]: Verifica se a lista está vazia
//RETORNO:
//		1 se lista está vazia
//		0 se lista não está vazia.
int M_isEmpty(MemItem *p){
	return (p->next==NULL)?1:0;
}

//[M_start]: Cria a lista de página alocadas para esse
//processo. Esse será a célula cabeça
void M_start(MemItem* p){
	p->next=NULL;
}

//[M_insere]: Cria uma nova entrada para uma página alocada.
//Ele insere na ordem dos VAddr
//RETORNO:
//		1 se deu tudo certo
//		0 se deu erro
int M_insere(MemItem *p, int VAddr, int RAMAddr, char Local, int Access, char Dirty, int DiskQuadroAddr, char AddrReady){
	MemItem *pnew = (MemItem *)malloc(sizeof(MemItem));

	if(!pnew)	 //Se não tem memória ou deu erro
		return 0;

	//Inicializa PNEW
	pnew->next=NULL;
	pnew->VAddr=VAddr;
	pnew->RAMAddr=RAMAddr;
	pnew->Local=Local;
	pnew->Access=Access;
	pnew->Dirty=Dirty;
	pnew->DiskQuadroAddr=DiskQuadroAddr;
	pnew->AddrReady=AddrReady;

	if(M_isEmpty(p))
		p->next=pnew;
	else{
		MemItem *tmp = p;
		while(tmp->next!=NULL && tmp->next->VAddr<VAddr)
			tmp=tmp->next;
		pnew->next=tmp->next;
		tmp->next=pnew;
	}
	return 1;
}

//[M_remove]: Remove uma entrada de uma página. 
//RETORNO
//		1 se removeu certinho (se existia na lista)
//		0 se deu erro
int M_remove(MemItem *p, int VAddr){
	MemItem *tmp = p;
	while(tmp->next!=NULL && tmp->next->VAddr!=VAddr)
		tmp=tmp->next;

	if(!tmp->next) //Se elemento não existe
		return 0;
	else{
		MemItem *remover = tmp->next;
		tmp->next=tmp->next->next;
		//Disk_remove(remover); //Desaloca o espaço do disco
		free(remover);
		return 1;
	}
}

//[M_isset]: Informa se a página está alocada ou não. Se estiver ele 
//já retorna o item de página
//RETORNO
//		*MemItem: Endereço da célula que guarda informações da página
//		alocada
//		
//		NULL: Se o página não existe
MemItem *M_isset(MemItem *p, int VAddr){
	MemItem *tmp = p;
	while(tmp->next!=NULL && tmp->next->VAddr!=VAddr)
		tmp=tmp->next;	
	return tmp->next;
}

//[M_libera]: Remove todas as entradas e recoloca na lista de endereços
//de memória e de disco que estão livres
void M_libera(MemItem *p){
	MemItem *tmp = p;
	while(tmp->next!=NULL){
		MemItem *prox_nome=tmp;
		//Disk_addlivre(tmp->DiskQuadroAddr);//Libera o quadro correspondente do disco.
		//if(tmp->Local==0) //Se o dado estava na memória ele libera ela
		//	Mem_addlivre(tmp->RAMAddr);
		free(tmp);
		tmp=prox_nome;
	}
}

								/*---- FIM DAS FUNÇÕES DA LISTA DE PÁGINAS DO PROCESSO ---- */








void pager_init(int nframes, int nblocks){
	printf("pager_init");
}	


void pager_create(pid_t pid){
	printf("pager_create");
}


void *pager_extend(pid_t pid){
	printf("pager_extend");
	return NULL;
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