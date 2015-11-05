#include <stdio.h>
#include <stdlib.h>
#include "pager.h"
#include "mmu.h"
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

	


		/*------- INÍNIO ESTRUTURA DE DADOS ------ */

//[Estrutura CELL]: Responsável em armazenar todas as informações de uma entrada
//de página do processo X.
struct Cell{
	//[VAddr]: Endereço Virtual
	intptr_t *VAddr; 		

	//[RAMAddr]: Endereço Real na Memória RAM
	int RAMAddr;	

	//[Local]:0 significa que o dado está na Memória, e 1 o dado está no disco
	char Local; 	

	//[Access]: Flag de acesso. Salva a última vez que acessamos o endereço X. 
	//Será usada na política de remoção da segunda chance
	char Access;		
	
	//[PermissaoAcesso]:Permite identificar quando o processo tentou escrever
	//na memória. Isso será útil para identificar quando o Dirty bit será 1.
	int PermissaoAcesso;

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
	//		1 = pront
	//		0=pendente ( ainda não foi alocado)
	char AddrReady;				
	
	//[next]: Ponteiro para o próximo elemento da lista				
    struct Cell *next;
};

typedef struct Cell MemItem;

//[Estrutura PROCESS]: Esta estrutura aponta para a lista de entradas de que 
//um processo tem.
struct Process{
	//[PID]: PID do processo
	pid_t PID;

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
	
	//[Free]: Diz se esse quadro/frame está sendo usado/reservado
	//	1 = true (SIM, está livre)
	//	0 = false (NAO, está ocupado)	
	int Free;

	//[PID]: PID do processo
	pid_t PID;


	//[next]: Ponteiro para a próxima posição livre
	struct freeMemory *next;  

	//[RAMAddr]: Ponteiro para RAM (para percorrer na segunda chance)
	MemItem *RAMAddr;
};

typedef struct freeMemory FMItem;

		/*---------FIM ESTRUTURA DE DADOS -------*/

		/*--------ESTRUTURAS GLOBAIS ----------*/

FMItem *listaMemoriaVazia;
FMItem *listaDiscoVazio;
PIDItem *listaProcessos;
FMItem *AddrSgundaChance=NULL;	//endereço usado para continuar de onde parou no alg. da segunda chance

		/*-------FIM ESTRUTURAS GLOBAIS -------*/
		/*------- CABEÇALHOS DAS FUNÇÕES --------*/

//Funções relacionadas aos processos
int P_isEmpty(PIDItem *p);
void P_start(PIDItem* p);
int P_insere(PIDItem *p, pid_t PID);
int P_remove(PIDItem *p, pid_t PID, FMItem *FreeMemory, FMItem *FreeDisk);
int P_isset(PIDItem *p, pid_t PID);
MemItem **P_getpages(PIDItem *p, pid_t PID);
int P_removeDaMemoria(PIDItem *p, MemItem *m, pid_t PID);

//Funções relacionadas às memórias que cada processo tem alocado
int M_isEmpty(MemItem *p);
void M_start(MemItem* p);
int M_isonRAM(MemItem *p);
int M_insere(MemItem **p, intptr_t *VAddr, int RAMAddr, char Local, char Access, char Dirty, int DiskQuadroAddr, char AddrReady);
int M_remove(MemItem **p, intptr_t *VAddr, FMItem *FreeMemory, FMItem *FreeDisk);
MemItem *M_isset(MemItem *p, intptr_t *VAddr);
void M_libera(MemItem *p, FMItem *FreeMemory, FMItem *FreeDisk);
intptr_t M_getFirsFreeAdd(MemItem *p);

//Funções relacionada ao banco de RAM e DISCO livres
int fM_isEmpty(FMItem *p);
void fM_start(FMItem* p);
int fM_insere(FMItem *p, int Addr);
int fM_reservaEspaco(FMItem *p, MemItem *m, pid_t PID);
void fM_libera(FMItem *p, int Addr);


		/*---------FIM CABECALHOS DE FUNÇÕES ------*/

		/*---FUNÇÕES DAS MEMÓRIAS LIVRES - DISCO E RAM ------*/

//[fM_isEmpty]: Verifica se a lista está vazia
//RETORNO:
//		True se lista está vazia
//		False se lista não está vazia.
int fM_isEmpty(FMItem *p){
	FMItem *tmp = p->next;
	
	while(tmp!=NULL && tmp->Free==0){
		tmp=tmp->next;
	}

	return (tmp==NULL);
}

//[fM_start]: Cria a lista de processos. Esse será a célula cabeça
void fM_start(FMItem* p){
	p->next=NULL;
}

//[fM_libera]: Função responsável em liberar quadro/frame p/ ser usado
void fM_libera(FMItem *p, int Addr){
	FMItem *tmp = p->next;
	while(tmp!=NULL && tmp->RealAddr!=Addr)
		tmp=tmp->next;
	if(tmp!=NULL)
		tmp->Free=1;

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
	pnew->Free=1;
	pnew->PID=0;
	pnew->RAMAddr=NULL;

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
int fM_reservaEspaco(FMItem *p, MemItem *m,pid_t PID){
	
	if(!fM_isEmpty(p))
	{

		FMItem *tmp = p->next;
		while(tmp->Free==0)
			tmp=tmp->next;
		tmp->Free=0;
		tmp->PID=PID;
		tmp->RAMAddr=m;//Faz a ligação da memória para os dados do 
		//p->next=p->next->next;
		int retorno=tmp->RealAddr;
		//free(remover);
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

//[P_start]: Cria a lista de processos. Esse será a célula cabeça
void P_start(PIDItem* p){
	p->next=NULL;
}

//[P_insere]: Cria uma nova entrada para um processo.
//Ele insere na ordem dos PIDS
//RETORNO:
//		1 se deu tudo certo
//		0 se deu erro
int P_insere(PIDItem *p, pid_t PID){
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
int P_remove(PIDItem *p, pid_t PID, FMItem *FreeMemory, FMItem *FreeDisk){
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
int P_isset(PIDItem *p, pid_t PID){
	PIDItem *tmp = p;
	while(tmp->next!=NULL && tmp->next->PID!=PID)
		tmp=tmp->next;
	return tmp->next!=NULL;
}

//[P_getpages]: Retorna o início da lista de páginas para o processo 
//RETORNO
//		MemItem *: Se existe alguma página para o processo
//		NULL: 		se nao existe nenhuma página para esse processo
MemItem **P_getpages(PIDItem *p, pid_t PID){
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
int P_removeDaMemoria(PIDItem *p, MemItem *m, pid_t PID){
	FMItem *tmp = listaMemoriaVazia->next;
	while(tmp!=AddrSgundaChance)
		tmp=tmp->next;


	while(1){
		
		MemItem *mem =  tmp->RAMAddr;
		if(mem->Local==0){
			if(mem->Access==0){//Esse item será removido da memória
				
				tmp->PID=PID;
				tmp->RAMAddr=m;//Faz a ligação da memória para os dados do 
				
				mem->Local=1; //Marca que está no disco
			
				if(mem->Dirty==1){
					mem->Dirty=0;
					mmu_disk_write(mem->RAMAddr, mem->DiskQuadroAddr);//Salva no disco
				
					mmu_nonresident(tmp->PID, (void *)*mem->VAddr);
					
				}
				AddrSgundaChance=(tmp->next==NULL)?listaMemoriaVazia->next:tmp->next;					
				return mem->RAMAddr;//Retorna o endereço real liberado

			}else{//Dá a segunda chance
				mem->Access=0;
				mem->PermissaoAcesso=PROT_NONE;
				mmu_chprot(tmp->PID, (void *)*mem->VAddr,PROT_NONE);//BLOQUEA PERMISSAO DESSA PÁGINA PARA ESSE PROCESSO! MOTIVO: SABER QUANDO O PROCESSO ACESSOU					
			}
		}
		
		//Esse if faz a ciclagem
		tmp=(tmp->next==NULL)?listaMemoriaVazia->next:tmp->next;
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
	return (p->AddrReady==1 && p->Local==0);
}

//[M_insere]: Cria uma nova entrada para uma página alocada.
//Ele insere na ordem dos VAddr
//RETORNO:
//		1 se deu tudo certo
//		0 se deu erro
int M_insere(MemItem **p, intptr_t *VAddr, int RAMAddr, char Local, char Access, char Dirty, int DiskQuadroAddr, char AddrReady){
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
		while(tmp->next!=NULL && *tmp->next->VAddr<*VAddr)
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
int M_remove(MemItem **p, intptr_t *VAddr, FMItem *FreeMemory, FMItem *FreeDisk){
	MemItem *tmp = *p;
	while(tmp->next!=NULL && *tmp->next->VAddr!=*VAddr)
		tmp=tmp->next;

	if(!tmp->next) //Se elemento não existe
		return 0;
	else{
		MemItem *remover = tmp->next;
		tmp->next=tmp->next->next;
		if(M_isonRAM(remover))//Se está na memória RAM
			fM_libera(FreeMemory,remover->DiskQuadroAddr);//Recoloca esse quadro na lista de vazios
			
		fM_libera(FreeDisk,remover->DiskQuadroAddr);//Recoloca esse quadro na lista de vazios
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
MemItem *M_isset(MemItem *p, intptr_t *VAddr){

	MemItem *tmp = p;
	while(tmp!=NULL && *tmp->VAddr!=*VAddr){
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
			fM_libera(FreeMemory,tmp->RAMAddr);//Recoloca esse quadro na lista de vazios
			
		fM_libera(FreeDisk,tmp->DiskQuadroAddr);//Recoloca esse quadro na lista de vazios
		free(tmp->VAddr);
		free(tmp);
		tmp=prox_nome;
	}
}


//M_getFirsFreeAdd]:Pega o primeiro endereço virtual disponível para o processo X
//		RETORNO: Endereço
intptr_t M_getFirsFreeAdd(MemItem *p){
	MemItem *tmp = p;
	intptr_t tmp_addr = UVM_BASEADDR;
	while(tmp!=NULL && tmp_addr==*tmp->VAddr){
		tmp=tmp->next;
		tmp_addr+=sysconf(_SC_PAGESIZE);
	}
	return tmp_addr;

}

		/*---- FIM DAS FUNÇÕES DA LISTA DE PÁGINAS DO PROCESSO ---- */

//[pager_init]: Repsonsável em incializar estruturas iniciais
void pager_init(int nframes, int nblocks){ 
	listaProcessos = (PIDItem *) malloc(sizeof(PIDItem));
	listaDiscoVazio = (FMItem *)malloc(sizeof(FMItem));
	listaMemoriaVazia=(FMItem *)malloc(sizeof(FMItem));
	P_start(listaProcessos);

	fM_start(listaDiscoVazio);
	fM_start(listaMemoriaVazia);
	
	int i;
	for(i=0;i <nframes;i++){ //Cria a lista de memória livres na RAM
		fM_insere(listaMemoriaVazia,i);
	}
	AddrSgundaChance=listaMemoriaVazia->next;
	
	for(i=0;i <nblocks;i++){ //Cria a lista de memória livres no DISCO
		fM_insere(listaDiscoVazio, i);
	}
}	

//[pager_create]: Responsável em add um processo. 
//DESC LONGA:
//			Basicamente ele add o processo na lista encadeada de processos
void pager_create(pid_t pid){
	P_insere(listaProcessos,pid);

}


void *pager_extend(pid_t pid){
	if(fM_isEmpty(listaDiscoVazio))
		return NULL;	

	int tmp_addr= fM_reservaEspaco(listaDiscoVazio,NULL,pid);//Espaço do disco que vai ser reservado
	intptr_t *tmp_new_addr = (intptr_t *)malloc(sizeof(intptr_t));//Endereço virtual para esse processo!

	*tmp_new_addr=M_getFirsFreeAdd(*P_getpages(listaProcessos, pid));

	M_insere(P_getpages(listaProcessos, pid),tmp_new_addr,0,0,0,0,tmp_addr,0);
	


	return (void *)*tmp_new_addr;

}


void pager_fault(pid_t pid, void *addr){
	

	
    intptr_t *AddrFiltro =  malloc(sizeof(intptr_t));
    *AddrFiltro = (intptr_t) addr;
    *AddrFiltro = (*AddrFiltro - (*AddrFiltro % sysconf(_SC_PAGESIZE)));
    
   	MemItem *m = M_isset(*P_getpages(listaProcessos, pid),(intptr_t *)AddrFiltro);
	if(!m->AddrReady){//Se ainda não foi alocado!
		int newMemAddr;
		if(fM_isEmpty(listaMemoriaVazia)){//Se memória está cheia!!
			newMemAddr = P_removeDaMemoria(listaProcessos,m,pid); //remove alguém da memória! Segunda chance já está implementado
		}else{
			newMemAddr = fM_reservaEspaco(listaMemoriaVazia, m,pid);//Retorna o primeiro elemento de memória livre!
		}

		m->RAMAddr=newMemAddr;
		m->Local=0; //Está na RAM
		m->Access=1; //Acessou!	
		m->Dirty=0;	 
		m->AddrReady=1;
		m->PermissaoAcesso=PROT_READ;

		mmu_resident(pid, (void *)*AddrFiltro,m->RAMAddr,PROT_READ);

		mmu_zero_fill(m->RAMAddr);	//zera endereços na memória!
		//CHAMAR FUNCAO ZERA PAGINA NA MEMÓRIA

	}else{//Se já está alocado!
		if(m->Local==1){//Se está no disco tenho que trazer da memória
			int newMemAddr;
			if(fM_isEmpty(listaMemoriaVazia))//Se memória está cheia!!
				{newMemAddr = P_removeDaMemoria(listaProcessos,m,pid);} //remove alguém da memória! Segunda chance já está implementado
			else
				{newMemAddr = fM_reservaEspaco(listaMemoriaVazia, m,pid);}//Retorna o primeiro elemento de memória livre!
			mmu_disk_read(m->DiskQuadroAddr, newMemAddr);//Pega do disco e traz para a memória
			m->Access=1;
			m->Local=0;
			m->RAMAddr=newMemAddr;
			m->Dirty=0;
			m->PermissaoAcesso=PROT_READ;
			mmu_resident(pid,(void *) *AddrFiltro,newMemAddr,PROT_READ);

			mmu_chprot(pid,(void *) *AddrFiltro,PROT_READ);//Define somente leitura para quando escrever eu saber!!!

		}else{ //Se está na memória deu erro foi de permissao!
			if(m->PermissaoAcesso==PROT_READ){//Só tem permissaõ de leitura, e o processo está tentando escreever
				m->Dirty=1;
				m->PermissaoAcesso=PROT_READ|PROT_WRITE;
				mmu_chprot(pid, (void *) *AddrFiltro,PROT_READ|PROT_WRITE);
			}else{//ele vai entrar aqui se ele não tem permissão de NADA 
				m->PermissaoAcesso=PROT_READ;
				mmu_chprot(pid, (void *) *AddrFiltro,PROT_READ); //Nesse momento o dirty bit estará zero. Então dou permissaõ somente de leitura. 
			}
			
			m->Access=1;//Usado na segunda chance
		}
	}
free(AddrFiltro);
}


int pager_syslog(pid_t pid, void *addr, size_t len){
	intptr_t *VirtualAddress = (intptr_t *)malloc(sizeof(intptr_t));
	*VirtualAddress= (intptr_t) addr;
	char *tmpstring=malloc(len);
	tmpstring[0] = '\0';
	size_t lentmp=len;
	
	if(*VirtualAddress%sysconf(_SC_PAGESIZE)!=0){
		long int startadd=*VirtualAddress;
		*VirtualAddress-=(*VirtualAddress%sysconf(_SC_PAGESIZE));
		lentmp-=(sysconf(_SC_PAGESIZE)-(*VirtualAddress%sysconf(_SC_PAGESIZE)));
	
		MemItem *m = M_isset(*P_getpages(listaProcessos, pid),VirtualAddress); 
		if(m==NULL)
	 		return -1;
	 	if(!m->AddrReady || m->Local==1 || m->PermissaoAcesso==PROT_NONE)
	 		pager_fault(pid, (void *)VirtualAddress);


	 	//Essa sequencia aloca o início
		char *phead = malloc(((sysconf(_SC_PAGESIZE)-(*VirtualAddress%sysconf(_SC_PAGESIZE)))>len)?len:(sysconf(_SC_PAGESIZE)-(*VirtualAddress%sysconf(_SC_PAGESIZE))));
		long int *tmpread=malloc(sizeof(long int));
		*tmpread= (long int) (pmem+m->RAMAddr*sysconf(_SC_PAGESIZE)+(startadd%sysconf(_SC_PAGESIZE)));

   	
   		memcpy(phead, (const void *) *tmpread,((sysconf(_SC_PAGESIZE)-(*VirtualAddress%sysconf(_SC_PAGESIZE)))>len)?len:(sysconf(_SC_PAGESIZE)-(*VirtualAddress%sysconf(_SC_PAGESIZE))));
   		strcat(tmpstring, (const void *) *tmpread);
	 	free(tmpread);
	 	free(phead);

	 	*VirtualAddress+=sysconf(_SC_PAGESIZE);

	 	if((long int)lentmp<0)
	 		lentmp=0;
	 	

	}


	long int quant = lentmp/sysconf(_SC_PAGESIZE);//Quantidade de páginas para serem lidas
	long int rest = lentmp % sysconf(_SC_PAGESIZE);//Quantidade de bytes a serem lidos da última página
	int i;
	
	for(i=0;i<quant;i++){//Loop para verificar se processo pode acessar os
		MemItem *m = M_isset(*P_getpages(listaProcessos, pid),VirtualAddress);  
		if(m==NULL)
	 		return -1;
	 	if(!m->AddrReady || m->Local==1 || m->PermissaoAcesso==PROT_NONE)
	 		pager_fault(pid, (void *)VirtualAddress);
	//Essa sequencia aloca o restante
		char *phead = malloc(sysconf(_SC_PAGESIZE));
		long int *tmpread=malloc(sizeof(long int));
		*tmpread= (long int) (pmem+m->RAMAddr*sysconf(_SC_PAGESIZE));
   		
   		memcpy(phead, (const void *) *tmpread,sysconf(_SC_PAGESIZE));
   		strcat(tmpstring, (const void *) *tmpread);
	 	free(tmpread);
	 	free(phead);


		*VirtualAddress+=sysconf(_SC_PAGESIZE);

	}



	if(rest!=0){
		MemItem *m = M_isset(*P_getpages(listaProcessos, pid),VirtualAddress);
		if(m==NULL)
	 		return -1;
	 	if(!m->AddrReady || m->Local==1 || m->PermissaoAcesso==PROT_NONE)
	 		pager_fault(pid, (void *)VirtualAddress);
		

	 	//Essa sequencia aloca o restante
		char *phead = malloc(rest);
		long int *tmpread=malloc(sizeof(long int));
		*tmpread= (long int) (pmem+m->RAMAddr*sysconf(_SC_PAGESIZE));
   		
   		memcpy(phead, (const void *) *tmpread,rest);
   		strcat(tmpstring, (const void *) *tmpread);
	 	free(tmpread);
	 	free(phead);
	}
	printf("pager_syslog pid %d %s\n", (int)pid, tmpstring);
	free(VirtualAddress);
	free(tmpstring);
	return 0;
}

//[pager_destroy]: Responsável em destruir um processo
//DESC LONGA:
//			Dentro da P_remove ele já libera e recoloca as 
//			memórias (RAM e DISCO) na lista de prontos
void pager_destroy(pid_t pid){
	P_remove(listaProcessos, pid,listaMemoriaVazia, listaDiscoVazio);

}

