/* dove implementiamo swap pool table, pager,I/O flash device*/
#include "supportL.h"
#include "../phase2/kernel.h" // devo includerlo per permettere a uTLBrefillHandler di accedere alle strutture dati globali di fase 2

//metto l'indirizzo suggerito sulle specifiche:
#define SWAP_POOL_START 0x20020000
swap_t swapPoolTable[POOLSIZE];
void atomicRefresh(swap_t* swapFrame,int frame,int validation);


int swapPoolSemaphore;
static int fifoPages; /** dato che serve per decidere il frame vittima: ogni volta che c'è un page fault, incremento la variabile e il frame vittima è fifoPages % POOLSIZE.
                         eg. diciamo che tutti i frame sono occupati e compare un pagefault: fifopages = 0 mod 16 => frame 0, incremento 1.... prossimo pagefault 1 mod 16 = 1, incremento,ecc--*/ 

/*funzioni da implementare*/
/**
 * inizializzazione della swap pool table 
 * I frame devono matchare la dimensione delle pagine(duh)
 * POOLSIZE == 2 volte il numero di processmax.
 * Primo frame a 0x20020000. Ogni frame corrisponde a 0x1000(abbiamo deciso che le pagine hanno dimensione 4096 byte), Quindi POOLSIZE = 0x10, 0x10*0x1000 = 0x10000(memoria occupata da tutti)
 * Primo indirizzo finita swap pool: 0x20030000
 **/
/** init deve inizializzare la tabella di swap il cui ruolo è tenere la relazione tra frame e  (asid,pagina)
 * I campi della swapPool table sono: asid, numero di pagina, e puntatore alla page_table_entry della pagina che occupa quel frame
 * La relazione tra l'indice della swapPoolTable i e il frame relativo resta fissa, ciò che cambia è la relazione tra la pagina inserita e il frame.
 */
void initSwapTable(){
    swapPoolSemaphore=1;
    fifoPages = 0;
    for(int i = 0; i < POOLSIZE; i++){
        swapPoolTable[i].sw_asid   = -1;
        swapPoolTable[i].sw_pageNo = -1; // ci metto un valore impossibile(sperem)
        swapPoolTable[i].sw_pte    = NULL;
    }

}
void trapHandler(support_t*spt){};



/** Dell'associazione: entrySwapPoolTable e frame se ne occuperà il pager, questa è una mini funzione che userà per associare gli indici(swpt) con i frame. */
unsigned int addressSwapPool(int i){
    if(i < 0 || i >= POOLSIZE){ // non dovremme mai verificarsi
        PANIC();
    }
    return SWAP_POOL_START +(i*PAGESIZE);
}

//funzioni I/O: lettura da device into mem e viceversa
int rwToMem(int frameVictim,int asid,int pageNo,int op);
//pager
/** 
 * -------------------------
 * Il pager viene chiamato ogni qual volta ci sia bisono di aggiornare le pagine in pageTable(caricamento da Backing store e scrittura in Backing Store)
 * Questo rende fifoPages ottimo per ciclare tra le pagine: inizia che sceglie il frame 0 che ospita la pagina caricata da più tempo e così via.
 */
void pager(){

    //prendo le informazioni necessarie per proseguire
    support_t* supportPTR = (support_t*)SYSCALL(GETSUPPORTPTR,0,0,0); // ottengo il puntatore alla struttura di supporto
    int cause = supportPTR->sup_exceptState[PGFAULTEXCEPT].cause; //ottengo l'eccezione
    cause = cause & CAUSE_EXCCODE_MASK; //estraggo dalla cause il valore che indica se pagefault o TLBmod
    if(cause == EXC_MOD){ //causa modifica tlb: queste costanti si trovano in /uriscv/cpu.h
        trapHandler(supportPTR); //skrr
    } 

    //gain del mutual access alla swap pool
    SYSCALL(PASSEREN,(int)&swapPoolSemaphore,0,0);  //in headers/const.h

    //determino la missing page(Forse si può direttamente fare una funzione poichè non è la prima volta che mi viene chiesto)
    unsigned int entryHi = supportPTR->sup_exceptState[PGFAULTEXCEPT].entry_hi;
    int missingVpn = (entryHi & GETPAGENO) >> VPNSHIFT; // questa è la pagina che voglio caricare dal FlashDev...

    //calcolo il numero di pagina: se pagina normale ottengo un numero compreso tra 0-30, se caso stack ottengo 0x3ffff che converto in 31
    // 0x3ffff = 0011 1111 1111 1111 1111 0000 0000 0000 con una pagina normale: 0x8005.0000 diventa: 0100 and 0011 = 0 0000 and 1111 per gli intermedi, 0101 and 1111 = 0101 => 0 0 0 0 5.
    // caso 0xBFFFF and 0x3FFFF = B = 1011 and 0011 = 0011 = 3 e gli altri tutti f quindi 0x3FFFF. 
    if(missingVpn == 0x3FFFF){ // è forse poco elegante ma fa il suo lavoro
        missingVpn=31;
    }

    int isFree = 0; //per distinguere se fare o no punto 8
    //devo trovare un frame da liberare: caso 1. esiste un frame vuoto, caso 2 devo eliminare una pagina
    int frameVictim = -1;
    for(int i = 0; i < POOLSIZE;i++){
        if(swapPoolTable[i].sw_asid == -1){
            frameVictim = i;
            isFree =1; 
            break;
        }      
    }
    // non è stato trovato un frame libero
    if(frameVictim == -1){ 
        frameVictim = fifoPages % POOLSIZE;
        fifoPages++;
    } 
    //indirizzo fisico del frame da killare
    unsigned int frameAddr = SWAP_POOL_START +(frameVictim * PAGESIZE);
    //punto 8-> solo se frame occupato. UPdate atomico pageT e TLB, write nel backing store. Devo inoltre 
    if(isFree != 1){
        atomicRefresh(&swapPoolTable[frameVictim],-1,0);
         /* Ora devo scrivere sul device DATA0 field con il corretto indirizzo di start del blocco da 4k: Il frameStartAddress*/
        int IOstatus =rwToMem(frameVictim,swapPoolTable[frameVictim].sw_asid,swapPoolTable[frameVictim].sw_pageNo,1);//scrivo la pagina da killare in memoria.
        if(IOstatus != READY){ //operazione non andata bene
            SYSCALL(VERHOGEN,(int)&swapPoolSemaphore,0,0);
            trapHandler(supportPTR);
        }
    }
    //leggo da flashdev nel frame
    int IOstatus = rwToMem(frameVictim,supportPTR->sup_asid,missingVpn,2);
    if(IOstatus != READY){ //operazione non andata bene
            SYSCALL(VERHOGEN,(int)&swapPoolSemaphore,0,0);
            trapHandler(supportPTR);
    }
    //punto 10: Aggiorno la entry della swapPoolTable
    swapPoolTable[frameVictim].sw_asid = supportPTR->sup_asid;
    swapPoolTable[frameVictim].sw_pageNo = missingVpn;
    swapPoolTable[frameVictim].sw_pte = &supportPTR->sup_privatePgTbl[missingVpn];
    //punto 11/12 atomic update, Valid on e aggiornare anche PFN corretto
    atomicRefresh(&swapPoolTable[frameVictim],frameAddr,1);   
    //step 13
    SYSCALL(VERHOGEN,(int)&swapPoolSemaphore,0,0);
    //step 14
    LDST(&supportPTR->sup_exceptState[PGFAULTEXCEPT]);
}

//validation = 0 invalido, validation = 1 valido, frame = -1 per valdation off
void atomicRefresh(swap_t* swapFrame,int frame,int validation){
    //disabilito interrupt
    setSTATUS(getSTATUS() & ~MSTATUS_MIE_MASK);
    
    if(validation == 0 && frame == -1){ // devo invalidare
        //devo individuare la pagina relativa da invalidare: swa_pte PTE==Page Table Entry Devo solo modificare il bit V VALIDON lo faccio con and e ~VALIDO
        swapFrame->sw_pte->pte_entryLO &= ~VALIDON;
    }else if(validation == 1){ //devo validare
        //questo funziona perchè frame , DIRTYON e VALIDON occupano bit diversi. frame è allineato con PAGESIZE(4096 o 0x1000) quindi ultimi 3 byte sono vuoti dove stanno i flag
        swapFrame->sw_pte->pte_entryLO = frame | DIRTYON | VALIDON;
    }
    // dovrei controllare se nella tlb questa pagina è conservata: primo approccio è cancellare tutto.
    TLBCLR();
    //riattivo interrupts
    setSTATUS(getSTATUS() | MSTATUS_MIE_MASK);
    
}
//in scrittura da ram a bs: prima invalido pag poi scrivo in bs
//in lettura da bs a ram: prima copio pagina nel frame(che al momento contiene una pagina V=0) poi modifico V = 1.
//si dice write/read in relazione all'operazione dal backingstore: asid 1-8 e flash 0-7
// op = 1 -> write 2->read, altri valori -> PANIC()
/**devAddrBase = START_DEVREG+ ((IntlineNo - 3) * 0x80)+ (DevNo * 0x10); Formula di phase2, intLineNo è 4(si trova in interrupts.c)
 * Deve ritornare lo status dell'op.
 * Op = 1 write Op = 2 read
*/
int rwToMem(int frameVictim,int asid,int pageNo,int op){
    if(op != 1 && op != 2){
        PANIC();
    }
    unsigned int ramAddr = addressSwapPool(frameVictim);
    //ora: come individuo il dispositivo? ogni uproc è associato con il suo flash-dev: IntLineNo dei flash dev = 4
    int devNumber = asid -1;
    unsigned int flashBase =((unsigned int) START_DEVREG +((4-3)*0x80)+(devNumber*0x10)); //indirizzo base del flashdev. Ora in base all'offset otteniamo gli indirizzi necessari
    unsigned int *DATA0addr=(unsigned int *)(flashBase + 0x08);
    unsigned int *commandAddr  =(unsigned int *)(flashBase + 0x04);
    //dichiaro il comando da scrivere
    unsigned int command;
    //scrivo sul registro DATA0 l'indirizzo del frame
    *DATA0addr = ramAddr;

    if(op == 1){//write
        command =( pageNo << 8) | FLASHWRITE; //in command least sig. byte è il comando: 8 equivale a lasciare libero un byte(quello del comando)
    }else if(op == 2){ //read
        command =(pageNo << 8)  | FLASHREAD;
    }
    //dopo aver caricato i dati corretti dico al kernel di eseguire la DOIO
    int ioStatus =SYSCALL(DOIO,(int)commandAddr,command,0);
    return ioStatus;
}



//pager: ottiene la pagina che vuole essere caricata, cerca se c'è un frame libero, se sì la carica e basta, altrimenti: deve selezionare il frame da killare, al suo interno c'è la pagina toKill
// ora dobbiamo: invalidare la pagina, scrivere toKill nel flashDevice,scrivere la pagina nuova toAdd nel frame, aggiornare i dati della swapPoolTable, aggiornare le tabelle. 