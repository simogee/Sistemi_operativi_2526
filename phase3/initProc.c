#include "supportL.h"

/* dove inizializziamo i processi utente
Primo: inizializziamo processo test/process_initiatior che dovrà:
    inizializzare strutture dati condivise: swap pool table, swap pool sem, device sem.
    Lanciare 1-8 processi utente
    Attende il termine dei processi lanciati.
Dobbiamo inizializzare le strutture dati condivise tra 
*/

support_t supportTable[UPROCMAX]; // tabella statica dove per ogni processo con ASID viene salvata la struttura support_t. Ogni PCB ci può accedere tramite support_t *p_supportStruct;

int masterSemaphore;
int shellSemaphore;

int flashSemaphore[UPROCMAX];

int readTermsemaphore;
int writeTermsemaphore;
// dall'asid seleziono il supportTable[asid-1] e poi guardo i campi della support_t(headers/types.h) e li aggiorno

//funzione per eliminare lo stato precedente del processo
void resetState(state_t* stato){
    stato->pc_epc = 0; //indirizzo istruzione corrente
    stato->cause = 0;  //indica causa dell'interrupt
    stato->mie = 0; //maschera interrupt abilitati: indica quali sorgenti di interrupt abilitati
    stato->entry_hi = 0;// VPN + asid
    for(int i = 0; i < 32; i++){ //registri
        stato->gpr[i]=0;
    }
    stato->status = 0; //status indica se la cpu è abilitata a ricevere gli interrupt e il modo: MPP-> modo di esecuzione, MIE-> interrupt globalmente abilitati,MPIE-> ripristino interrupt dopo eccezione(ricorda pre eccezione)
                       // per modifiche si usano maschere di bit
}

//come scritto nelle side: solo sup_asid,sup_exceptContext[2], and sup_privatePgTbl[32] richiedono init prima di richiesta di creazione processo
void initSupportStructure(int asid){
    support_t* supportProc = &supportTable[asid-1];
    supportProc->sup_asid = asid;

    supportProc->sup_exceptContext[PGFAULTEXCEPT].pc = &pager;
    supportProc->sup_exceptContext[PGFAULTEXCEPT].status =MSTATUS_MPP_M | MSTATUS_MIE_MASK; //kernel mode con tutti gli interrupt abilitati
    supportProc->sup_exceptContext[PGFAULTEXCEPT].stackPtr=&(supportProc->sup_stackTLB[499]);
    supportProc->sup_exceptContext[GENERALEXCEPT].pc = &generalExceptionHandler;
    supportProc->sup_exceptContext[GENERALEXCEPT].status =MSTATUS_MPP_M | MSTATUS_MIE_MASK;
    supportProc->sup_exceptContext[GENERALEXCEPT].stackPtr=&(supportProc->sup_stackGen[499]);

    /*indirizzo base e stack*/
    unsigned int start_addr=0x80000;
    unsigned int stack_addr=0xBFFFF;

    //inizializzo le prime 31 pagine
    for(int i = 0; i < USERPGTBLSIZE -1; i++){
        unsigned int VPN = start_addr +i;

        supportProc->sup_privatePgTbl[i].pte_entryHI=(VPN << VPNSHIFT)|(asid << ASIDSHIFT);
        supportProc->sup_privatePgTbl[i].pte_entryLO=DIRTYON;
    }
    //pagina stack
    supportProc->sup_privatePgTbl[USERPGTBLSIZE-1].pte_entryHI=(stack_addr<<VPNSHIFT)|(asid<< ASIDSHIFT);
    supportProc->sup_privatePgTbl[USERPGTBLSIZE-1].pte_entryLO=DIRTYON;

}


void initDevSemaphore(int fls_dev){
    for(int i = 0; i< UPROCMAX;i++){
        flashSemaphore[i] = 1;
    }
}
//prende da support table supportTable[asid-1],inizializza la support struct(initSupportStructure), prepara lo state iniziale-> registri puntati correttamente, user mode, interrupt abilitati, asid in entry_hi e chiama create process(Kernel)
void processCreation(int asid){

}
/**inizializza swap pool table e semaforo, inizializza tutti i semafori, crea processo shell, fa P su masterSemaphore e poi TermProcess(kernel) */
void test(){
    masterSemaphore = 0;
    shellSemaphore = 0;

    readTermsemaphore = 1;
    writeTermsemaphore = 1;

}