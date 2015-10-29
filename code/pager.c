#include <stdio.h>
#include <stdlib.h>
#include "pager.h"
#include "mmu.h"
#include <unistd.h>
#include <sys/mman.h>

	


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

	//[next]: Ponteiro para a próxima posição livre
	struct freeMemory *next;  //PENSAR COMO RESOLVER ISSO! DEPOIS EXPLICO ONDE QUE EStÁ ERRADO

	//[RAMAddr]: Ponteiro para RAM (para percorrer na segunda chance)
	MemItem *RAMAddr;
};

typedef struct freeMemory FMItem;

		/*---------FIM ESTRUTURA DE DADOS -------*/

		/*--------ESTRUTURAS GLOBAIS ----------*/

FMItem *listaMemoriaVazia;
FMItem *listaDiscoVazio;
PIDItem *listaProcessos;
int AddrSgundaChance=-1;	//endereço usado para continuar de onde parou no alg. da segunda chance

		/*-------FIM ESTRUTURAS GLOBAIS -------*/
		/*------- CABEÇALHOS DAS FUNÇÕES --------*/

//Funções relacionadas aos processos
int P_isEmpty(PIDItem *p);
void P_start(PIDItem* p);
int P_insere(PIDItem *p, pid_t PID);
int P_remove(PIDItem *p, pid_t PID, FMItem *FreeMemory, FMItem *FreeDisk);
int P_isset(PIDItem *p, pid_t PID);
MemItem **P_getpages(PIDItem *p, pid_t PID);
int P_removeDaMemoria(PIDItem *p);

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
int P_removeDaMemoria(PIDItem *p){
	PIDItem *tmp = p->next;
	while(1){
		MemItem *mem = tmp->mem;
		while(mem!=NULL && mem->AddrReady==1){

			if(mem->Local==0){//Se está na memória
				if(mem->Access==0){//Esse item será removido da memória
					printf("vai remover o endereço %d da memória. Local=%d Dirty=%d\n",mem->RAMAddr,mem->Local, mem->Dirty );
					mem->Local=1; //Marca que está no disco
				
					if(mem->Dirty==1){
						printf("ESTA SUJO! Temos que salvar no disco\n");
						mem->Dirty=0;
						mmu_disk_write(mem->RAMAddr, mem->DiskQuadroAddr);//Salva no disco
						
					}
					printf("vai remover o endereço %d da memória. Local=%d Dirty=%d\n",mem->RAMAddr,mem->Local, mem->Dirty );
					return mem->RAMAddr;//Retorna o endereço real liberado

				}else{//Dá a segunda chance
					mem->Access=0;
					mmu_chprot(tmp->PID, (void *)*mem->VAddr,PROT_NONE);//BLOQUEA PERMISSAO DESSA PÁGINA PARA ESSE PROCESSO! MOTIVO: SABER QUANDO O PROCESSO ACESSOU					
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
MemItem *M_isset(MemItem *p, intptr_t *VAddr){

	MemItem *tmp = p;
if(tmp==NULL){
	printf(">>>>>>>>>NAO TEM NADA ALOCADO PARA ESSE PROCESSO\n");
}

	while(tmp!=NULL && *tmp->VAddr!=*VAddr){
	    printf("M_isset Vaddr: %ld == %ld\n", *tmp->VAddr,*VAddr);
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


//M_getFirsFreeAdd]:Pega o primeiro endereço virtual disponível para o processo X
//		RETORNO: Endereço
intptr_t M_getFirsFreeAdd(MemItem *p){
	MemItem *tmp = p;
	intptr_t tmp_addr = UVM_BASEADDR;
	printf("BASEDIR: %ld\n",tmp_addr );

	while(tmp!=NULL && tmp_addr==*tmp->VAddr){
		printf("\t\t---ENTROU\n");
		tmp=tmp->next;
		tmp_addr+=sysconf(_SC_PAGESIZE);
	}
	return tmp_addr;

}

		/*---- FIM DAS FUNÇÕES DA LISTA DE PÁGINAS DO PROCESSO ---- */





///FUNÇÕES PARA DEBUG
void listaTudo(PIDItem *p){
	PIDItem *tmp = p->next;
	while(tmp!=NULL){
		printf("->%d\n", tmp->PID);
		MemItem *mem = tmp->mem;
		while(mem!=NULL){
			printf("\tVAddr: %ld RAMAddr: %d Local: %d Permissao: %d Ready: %d Dirty: %d DiskQuadroAddr: %d Access: %d\n", *mem->VAddr,mem->RAMAddr,mem->Local, mem->PermissaoAcesso, mem->AddrReady, mem->Dirty, mem->DiskQuadroAddr, mem->Access );
			mem=mem->next;
		}
		printf("END LISTA TUDO\n");
		tmp=tmp->next;
	}
}


void listaMemLivre(FMItem *p){
	printf("MEMória Livre\n");
	FMItem *tmp = p->next;
	while(tmp!=NULL){
		printf("->%d\n", tmp->RealAddr);
		
		tmp=tmp->next;
	}
}

///FIM FUNCOES DEBUG




//[pager_init]: Repsonsável em incializar estruturas iniciais
void pager_init(int nframes, int nblocks){ 
	printf("----------------------------- PAGER INIT\n");

	listaProcessos = (PIDItem *) malloc(sizeof(PIDItem));
	listaDiscoVazio = (FMItem *)malloc(sizeof(FMItem));
	listaMemoriaVazia=(FMItem *)malloc(sizeof(FMItem));
	P_start(listaProcessos);

	fM_start(listaDiscoVazio);
	fM_start(listaMemoriaVazia);
	AddrSgundaChance=-1;
	
	int i;
	for(i=0;i <nframes;i++){ //Cria a lista de memória livres na RAM
		fM_insere(listaMemoriaVazia,i);
	}

	for(i=0;i <nblocks;i++){ //Cria a lista de memória livres no DISCO
		fM_insere(listaDiscoVazio, i);
	}
printf("----------------------------------end init\n\n");
}	

//[pager_create]: Responsável em add um processo. 
//DESC LONGA:
//			Basicamente ele add o processo na lista encadeada de processos
void pager_create(pid_t pid){
		printf("------------------------------------PAGER CREATE");
		P_insere(listaProcessos,pid);

	listaTudo(listaProcessos);
	listaMemLivre(listaMemoriaVazia);
	listaMemLivre(listaDiscoVazio);
	printf("-----------------------------------------end PAGER CREATE\n\n");
}


void *pager_extend(pid_t pid){
	printf("--------------------------------------PAGER EXTEND\n");
	if(fM_isEmpty(listaDiscoVazio))
		return NULL;	

	int tmp_addr= fM_reservaEspaco(listaDiscoVazio);//Espaço do disco que vai ser reservado
	intptr_t *tmp_new_addr = (intptr_t *)malloc(sizeof(intptr_t));//Endereço virtual para esse processo!

	*tmp_new_addr=M_getFirsFreeAdd(*P_getpages(listaProcessos, pid));
	printf("NEW VADDr: %ld\n", *tmp_new_addr);

	M_insere(P_getpages(listaProcessos, pid),tmp_new_addr,0,0,0,0,tmp_addr,0);
	

	listaTudo(listaProcessos);
	listaMemLivre(listaMemoriaVazia);
	listaMemLivre(listaDiscoVazio);
	printf("----------------------------------end PAGER EXTEND\n\n");

	return (void *)*tmp_new_addr;//SIM! TEM QUE TER ESSES 2 ASTERISCOS PORRA! Deu uma trabalheira descobrir essa merda que você nem imagina

}


void pager_fault(pid_t pid, void *addr){
	printf("--------------------------------PAGER FAULT\n");
	if(AddrSgundaChance==-1){  //Inicializa contador de memória do Segunda Chance
		AddrSgundaChance=listaMemoriaVazia->next->RealAddr;
	}


    intptr_t *tmpa;
    tmpa = (intptr_t *)addr;

	MemItem *m = M_isset(*P_getpages(listaProcessos, pid),(intptr_t *)&addr); //Esse porra também deu trabalho descobrir 

	if(!m->AddrReady){//Se ainda não foi alocado!
		printf("\t\t\tpager fault  - AINDA NAO FOI ALOCADO");
		int newMemAddr;
		if(fM_isEmpty(listaMemoriaVazia)){//Se memória está cheia!!
			printf("\t\t\tpager fault - MEMORIA CHEIA\n");
			newMemAddr = P_removeDaMemoria(listaProcessos); //remove alguém da memória! Segunda chance já está implementado
		}else{
			printf("\t\t\tpager fault - MEMORIA VAZIA\n");
			newMemAddr = fM_reservaEspaco(listaMemoriaVazia);//Retorna o primeiro elemento de memória livre!
		}

		m->RAMAddr=newMemAddr;
		m->Local=0; //Está na RAM
		m->Access=1; //Acessou!	
		m->Dirty=0;	 
		m->AddrReady=1;
		m->PermissaoAcesso=PROT_READ;

		mmu_resident(pid,addr,m->RAMAddr,PROT_READ);

		mmu_zero_fill(m->RAMAddr);	//zera endereços na memória!
		//CHAMAR FUNCAO ZERA PAGINA NA MEMÓRIA

	}else{//Se já está alocado!
		printf("\t\t\t\t já foi alocado!\n");
		if(m->Local==1){//Se está no disco tenho que trazer da memória
			printf("ESTA NO DISCO VEI\n");
			int newMemAddr;
			if(fM_isEmpty(listaMemoriaVazia))//Se memória está cheia!!
				newMemAddr = P_removeDaMemoria(listaProcessos); //remove alguém da memória! Segunda chance já está implementado
			else
				newMemAddr = fM_reservaEspaco(listaMemoriaVazia);//Retorna o primeiro elemento de memória livre!
			printf("SERA COLOCADO NA POSICAO %d\n",newMemAddr );
			mmu_disk_read(m->DiskQuadroAddr, newMemAddr);//Pega do disco e traz para a memória
			m->Access=1;
			m->Local=0;
			m->RAMAddr=newMemAddr;
			m->Dirty=0;
m->PermissaoAcesso=PROT_READ;
			mmu_chprot(pid,addr,PROT_READ);//Define somente leitura para quando escrever eu saber!!!

		}else{ //Se está na memória deu erro foi de permissao!
			printf("ESTA NA RAM\n");
			if(m->PermissaoAcesso==PROT_READ){//Só tem permissaõ de leitura, e o processo está tentando escreever
				printf("SO LEITURA-AGORA ELE PODE ESCREVER\n");
				m->Dirty=1;
				m->PermissaoAcesso=PROT_READ|PROT_WRITE;
				mmu_chprot(pid,addr,PROT_READ|PROT_WRITE);
			}else{//ele vai entrar aqui se ele não tem permissão de NADA (ou seja, quando retornar do disco p/ a RAM)
				printf("acho que nunca vai entrar aqui.\n");
				mmu_chprot(pid,addr,PROT_READ); //Nesse momento o dirty bit estará zero. Então dou permissaõ somente de leitura. 
			}
			
			m->Access=1;//Usado na segunda chance
		}
	}
listaTudo(listaProcessos);
listaMemLivre(listaMemoriaVazia);
listaMemLivre(listaDiscoVazio);
printf("----------------------------------- END PAGE FAULT \n");
}


int pager_syslog(pid_t pid, void *addr, size_t len){
	printf("pager_syslog");
	return 0;
}

//[pager_destroy]: Responsável em destruir um processo
//DESC LONGA:
//			Dentro da P_remove ele já libera e recoloca as 
//			memórias (RAM e DISCO) na lista de prontos
void pager_destroy(pid_t pid){
	printf("PAGER DESTROY\n");
	P_remove(listaProcessos, pid,listaMemoriaVazia, listaDiscoVazio);
	listaTudo(listaProcessos);
	listaMemLivre(listaMemoriaVazia);
	listaMemLivre(listaDiscoVazio);
	printf("end PAGER DESTROY\n\n");
}


