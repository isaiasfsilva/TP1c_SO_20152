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
	char Access;		
					
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
	MemItem *mem;

	//[next]: Ponteiro para o próximo processo
	struct Process *next;
};

typedef struct Process PIDItem;

//[Estrutura FREEMEMORY]: Esta estrutura é responsável em dizer quais espaços
//da memória ou disco estão vazios para serem alocados. Só!
struct freeMemory{

	//[RealAddr]: Endereço real da posição vazia!
	int RealAddr;

	//[next]: Ponteiro para a próxima posição livre
	struct freeMemory *next;
};

typedef struct freeMemory FMItem;

		/*---------FIM ESTRUTURA DE DADOS -------*/

		/*--------ESTRUTURAS GLOBAIS ----------*/

FMItem *listaMemoriaVazia;
FMItem *listaDiscoVazio;
PIDItem *listaProcessos;

		/*-------FIM ESTRUTURAS GLOBAIS -------*/
		/*------- CABEÇALHOS DAS FUNÇÕES --------*/

//Funções relacionadas aos processos
int P_isEmpty(PIDItem *p);
void P_start(PIDItem* p);
int P_insere(PIDItem *p, int PID);
int P_remove(PIDItem *p, int PID, FMItem *FreeMemory, FMItem *FreeDisk);
int P_isset(PIDItem *p, int PID);
MemItem **P_getpages(PIDItem *p, int PID);
int P_removeDaMemoria(PIDItem *p);

//Funções relacionadas às memórias que cada processo tem alocado
int M_isEmpty(MemItem *p);
void M_start(MemItem* p);
int M_isonRAM(MemItem *p);
int M_insere(MemItem **p, int VAddr, int RAMAddr, char Local, char Access, char Dirty, int DiskQuadroAddr, char AddrReady);
int M_remove(MemItem **p, int VAddr, FMItem *FreeMemory, FMItem *FreeDisk);
MemItem *M_isset(MemItem *p, int VAddr);
void M_libera(MemItem *p, FMItem *FreeMemory, FMItem *FreeDisk);

//Funções relacionada ao banco de RAM e DISCO livres
int fM_isEmpty(FMItem *p);
void fM_start(FMItem* p);
int fM_insere(FMItem *p, int Addr);
int fM_reservaEspaco(FMItem *p);



		/*---------FIM CABECALHOS DE FUNÇÕES ------*/

		/*---FUNÇÕES DAS MEMÓRIAS LIVRES - DISCO E RAM ------*/

//[fM_isEmpty]: Verifica se a lista está vazia
//RETORNO:
//		True se lista está vazia
//		False se lista não está vazia.
int fM_isEmpty(FMItem *p){
	return (p->next==NULL);
}

//[fM_start]: Cria a lista de processos. Esse será a célula cabeça
void fM_start(FMItem* p){
	p->next=NULL;
}

//[fM_insere]: Cria uma nova entrada de uma memória livre.
//Ele insere na ordem dos Addr
//RETORNO:
//		1 se deu tudo certo
//		0 se deu erro
int fM_insere(FMItem *p, int Addr){
	FMItem *pnew = (FMItem *)malloc(sizeof(FMItem));

	if(!pnew)	 //Se não tem memória ou deu erro
		return 0;

	//Inicializa PNEW
	pnew->next=NULL;
	pnew->RealAddr=Addr;

	if(fM_isEmpty(p))
		p->next=pnew;
	else{
		FMItem *tmp = p;
		while(tmp->next!=NULL && tmp->next->RealAddr<Addr)
			tmp=tmp->next;
		pnew->next=tmp->next;
		tmp->next=pnew;
	}
	return 1;
}


//[fM_reservaEspaco]: Remove uma entrada de memória livre. 
//OBS: Pelo determinismo, ele SEMPRE retorna o menor endereço disponível
//RETORNO
//		RealAddr: Endereço do item removido que será alocado para algum processo
//		0 se deu erro 
int fM_reservaEspaco(FMItem *p){
	
	if(!fM_isEmpty(p))
	{
		FMItem *remover = p->next;
		p->next=p->next->next;
		int retorno=remover->RealAddr;
		free(remover);
		return retorno;
	}else
		return 0;
	
}


		/*---FIM DAS FUNÇÕES DAS MEMÓRIAS LIVRES - DISCO E RAM ------*/


		/*----[FUNÇÕES DA LISTA DE PROCESSOS]----*/



//[P_isEmpty]: Verifica se a lista está vazia
//RETORNO:
//		True se lista está vazia
//		False se lista não está vazia.
int P_isEmpty(PIDItem *p){
	return (p->next==NULL);
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
int P_remove(PIDItem *p, int PID, FMItem *FreeMemory, FMItem *FreeDisk){
	PIDItem *tmp = p;
	while(tmp->next!=NULL && tmp->next->PID!=PID)
		tmp=tmp->next;

	if(!tmp->next) //Se elemento não existe
		return 0;
	else{
		PIDItem *remover = tmp->next;
		tmp->next=tmp->next->next;

		M_libera(remover->mem, FreeMemory, FreeDisk);//Remove todas as entradas da memória
		free(remover);
		return 1;
	}
}

//[P_isset]: Informa se o processo existe ou não. 
//RETORNO
//		True: se o proceesso existe
//		
//		False: Se o processo não existe
int P_isset(PIDItem *p, int PID){
	PIDItem *tmp = p;
	while(tmp->next!=NULL && tmp->next->PID!=PID)
		tmp=tmp->next;
	return tmp->next!=NULL;
}

//[P_getpages]: Retorna o início da lista de páginas para o processo 
//RETORNO
//		MemItem *: Se existe alguma página para o processo
//		NULL: 		se nao existe nenhuma página para esse processo
MemItem **P_getpages(PIDItem *p, int PID){
	if(P_isEmpty(p))
		return NULL;

	PIDItem *tmp = p;
	while(tmp->next!=NULL && tmp->next->PID!=PID)
		tmp=tmp->next;

    
	return &(tmp->next->mem);
}

//[P_removedamemória]: Função responsável em escolher alguém para ser
//retirado da memória. 
//RETORNO:
//		RAMAddr: Retorna o endereço REAL que acabou de ser removido
int P_removeDaMemoria(PIDItem *p){
	PIDItem *tmp = p->next;
	while(1){
		MemItem *mem = tmp->mem;
		while(mem!=NULL){
			if(mem->Local==0){//Se está na memória
				if(mem->Access==0){//Esse item será removido da memória
					mem->Local=1; //Marca que está no disco
					if(mem->Dirty==1){
						//[uvm_save_on_disk]//Salva item no disco 				---FALTA IMPLEMENTAR!!!
					}
					return mem->RAMAddr;//Retorna o endereço real liberado

				}else{//Dá a segunda chance
					mem->Access=0;
				}
			}
			mem=mem->next;
		}
		//Esse if faz a ciclagem
		tmp=(tmp->next==NULL)?p->next:tmp->next;
	}
}
		/*--------- FIM FUNÇÕES LISTA DE PROCESSOS----------*/




		/*---- FUNÇÕES DA LISTA DE PÁGINAS DO PROCESSO ---- */



//[M_isEmpty]: Verifica se a lista está vazia
//RETORNO:
//		1 se lista está vazia
//		0 se lista não está vazia.
int M_isEmpty(MemItem *p){
	return (p==NULL);
}

//[M_start]: Cria a lista de página alocadas para esse
//processo. Esse será a célula cabeça
void M_start(MemItem* p){
	p->next=NULL;
}

//[M_isonRAM]: Diz se a página está na memória ram ou no disco
//RETORNO
//		true: se está na RAM
//		false: se está no disco
int M_isonRAM(MemItem *p){
	return (p->Local==0);
}

//[M_insere]: Cria uma nova entrada para uma página alocada.
//Ele insere na ordem dos VAddr
//RETORNO:
//		1 se deu tudo certo
//		0 se deu erro
int M_insere(MemItem **p, int VAddr, int RAMAddr, char Local, char Access, char Dirty, int DiskQuadroAddr, char AddrReady){
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
	if(M_isEmpty(*p)){

		*p=pnew;

	}
	else{
		MemItem *tmp = *p;
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
int M_remove(MemItem **p, int VAddr, FMItem *FreeMemory, FMItem *FreeDisk){
	MemItem *tmp = *p;
	while(tmp->next!=NULL && tmp->next->VAddr!=VAddr)
		tmp=tmp->next;

	if(!tmp->next) //Se elemento não existe
		return 0;
	else{
		MemItem *remover = tmp->next;
		tmp->next=tmp->next->next;
		if(M_isonRAM(remover))//Se está na memória RAM
			fM_insere(FreeMemory,remover->DiskQuadroAddr);//Recoloca esse quadro na lista de vazios
			
		fM_insere(FreeDisk,remover->DiskQuadroAddr);//Recoloca esse quadro na lista de vazios
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
	while(tmp!=NULL && tmp->VAddr!=VAddr){

		tmp=tmp->next;	
	}

	return tmp;
}

//[M_libera]: Remove todas as entradas e recoloca na lista de endereços
//de memória e de disco que estão livres
void M_libera(MemItem *p, FMItem *FreeMemory, FMItem *FreeDisk){
	MemItem *tmp = p;
	while(tmp!=NULL){
		MemItem *prox_nome=tmp->next;
		if(M_isonRAM(tmp))//Se está na memória RAM
			fM_insere(FreeMemory,tmp->DiskQuadroAddr);//Recoloca esse quadro na lista de vazios
			
		fM_insere(FreeDisk,tmp->DiskQuadroAddr);//Recoloca esse quadro na lista de vazios
		free(tmp);
		tmp=prox_nome;
	}
}

		/*---- FIM DAS FUNÇÕES DA LISTA DE PÁGINAS DO PROCESSO ---- */

//Função só para printar.

void listaTudo(PIDItem *p){
	PIDItem *tmp = p->next;
	while(tmp!=NULL){
		printf("->%d\n", tmp->PID);
		MemItem *mem = tmp->mem;
		while(mem!=NULL){
			printf("\tVAddr: %d Access: %d\n", mem->VAddr,mem->Access );
			mem=mem->next;
		}
		tmp=tmp->next;
	}
}


void listaMemLivre(FMItem *p){
	FMItem *tmp = p->next;
	while(tmp!=NULL){
		printf("->%d\n", tmp->RealAddr);
		
		tmp=tmp->next;
	}
}



int main(){
	// PIDItem *listaPID = (PIDItem *) malloc(sizeof(PIDItem));
	// if(!listaPID){
	// 	printf("Erro na criação da lista PID\n");
	// 	exit(1);
	// }

	// P_start(listaPID);
	// P_insere(listaPID, 10);
	// P_insere(listaPID, 11);
	// P_insere(listaPID, 12);
	// P_insere(listaPID, 13);
	// P_insere(listaPID, 14);

	
	// MemItem **mem=P_getpages(listaPID,12);
 //    M_insere(mem, 50,55,0,1,'1',550,'1');
 //    M_insere(mem, 51,56,0,1,'1',551,'1');
 //    M_insere(mem, 52,57,0,0,'1',552,'1');

	// mem=P_getpages(listaPID,14);
 //    M_insere(mem, 58,55,0,1,'1',550,'1');
 //    M_insere(mem, 59,56,0,1,'1',551,'0');
 //    M_insere(mem, 60,57,0,1,'1',552,'0');

	// mem=P_getpages(listaPID,10);
 //    M_insere(mem, 111,55,0,1,'1',550,'0');
 //    M_insere(mem, 112,56,0,1,'1',551,'0');
 //    M_insere(mem, 113,57,0,1,'1',552,'0');

 //   // M_remove(mem,51);
 //    listaTudo(listaPID);

 //    //MemItem *p, int VAddr, int RAMAddr, char Local, char Access, char Dirty, int DiskQuadroAddr, char AddrReady
	// P_removeDaMemoria(listaPID);
 //     listaTudo(listaPID);
	// P_remove(listaPID,10);
 //    P_remove(listaPID,11);
 //    P_remove(listaPID,13);
 //    P_remove(listaPID,14);

	


	// free(listaPID);
	
	// listaMemoriaVazia = (FMItem *) malloc(sizeof(FMItem));

	// fM_start(listaMemoriaVazia);
	// fM_insere(listaMemoriaVazia,10);
	// fM_insere(listaMemoriaVazia,11);
	// fM_insere(listaMemoriaVazia,12);	fM_insere(listaMemoriaVazia,16);
	// fM_insere(listaMemoriaVazia,13);
	// fM_insere(listaMemoriaVazia,14);
	// fM_insere(listaMemoriaVazia,15);

	// fM_insere(listaMemoriaVazia,17);
	// fM_insere(listaMemoriaVazia,18);
	// listaMemLivre(listaMemoriaVazia);

	// printf("REMOVEU: %d\n", fM_reservaEspaco(listaMemoriaVazia) );

	// printf("REMOVEU: %d\n", fM_reservaEspaco(listaMemoriaVazia) );
	// printf("REMOVEU: %d\n", fM_reservaEspaco(listaMemoriaVazia) );

	// printf("REMOVEU: %d\n", fM_reservaEspaco(listaMemoriaVazia) );

	// printf("REMOVEU: %d\n", fM_reservaEspaco(listaMemoriaVazia) );
	// printf("REMOVEU: %d\n", fM_reservaEspaco(listaMemoriaVazia) );
	// printf("REMOVEU: %d\n", fM_reservaEspaco(listaMemoriaVazia) );

	// printf("REMOVEU: %d\n", fM_reservaEspaco(listaMemoriaVazia) );
	// printf("REMOVEU: %d\n", fM_reservaEspaco(listaMemoriaVazia) );
	// printf("REMOVEU: %d\n", fM_reservaEspaco(listaMemoriaVazia) );

	// printf("REMOVEU: %d\n", fM_reservaEspaco(listaMemoriaVazia) );
	// printf("REMOVEU: %d\n", fM_reservaEspaco(listaMemoriaVazia) );
	// printf("REMOVEU: %d\n", fM_reservaEspaco(listaMemoriaVazia) );

	// printf("REMOVEU: %d\n", fM_reservaEspaco(listaMemoriaVazia) );
	// printf("REMOVEU: %d\n", fM_reservaEspaco(listaMemoriaVazia) );
	// listaMemLivre(listaMemoriaVazia);
	// 
	printf("%d\n", sizeof(pid_t) );
	return 0;
}