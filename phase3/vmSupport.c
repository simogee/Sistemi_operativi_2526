/* dove implementiamo swap pool table, pager,I/O flash device*/
#include "supportL.h"


//metto l'indirizzo suggerito sulle specifiche:

swap_t swapPoolTable[POOLSIZE];
void atomicRefresh(swap_t* swapFrame,int frame,int validation);
int vpnToPage(int vpn);


int swapPoolSemaphore;
static int fifoPages; /** dato che serve per decidere il frame vittima: ogni volta che c'è un page fault, incremento la variabile e il frame vittima è fifoPages % POOLSIZE.
                         eg. diciamo che tutti i frame sono occupati e compare un pagefault: fifopages = 0 mod 16 => frame 0, incremento 1.... prossimo pagefault 1 mod 16 = 1, incremento,ecc--*/ 
/*funzioni da implementare*/
/**
 * inizializzazione della swap pool table 
 * I frame devono matchare la dimensione delle pagine
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




/** Dell'associazione: entrySwapPoolTable e frame se ne occuperà il pager, questa è una mini funzione che userà per associare gli indici(swpt) con i frame. */
unsigned int addressSwapPool(int i){
    if(i < 0 || i >= POOLSIZE){ // non dovremme mai verificarsi
        PANIC(); // forse meglio syscall2?
    }
    return SWAP_POOL_START +(i*PAGESIZE);
}

//funzioni I/O: lettura da device into mem e viceversa
int rwToMem(int frameVictim,int asid,int pageNo,int op);
//pager
/** 
 * -------------------------
 * Il pager viene chiamato quando si verifica un pageFault. l'iter è il seguente: processo genera indirizzo virtuale, si cerca in tlb: se presente [----], se non è presente si va a guardare nella pageTable: 
 * qui si guarda il bit V: V = 1 valid tutto ok, bit V = 0 allora la pagina è mancante e va caricata dal backing store. Dobbiamo eliminare una pagina per fare spazio alla missing, selezioniamo con un algoritmo(FIFO qui)
 * invalidiamo le entry con V= 0(sia pagetable che TLB) e scriviamo nel backingstore la pagina vittima. poi leggiamo dal backingstore la pagina missing, e una volta finito validiamo V=1.
 * Una volta terminato carichiamo di nuovo lo stato precedente ma questa volta non si verificherà il pageFault e potremo proseguire
 * 
 */
void pager(){

    //prendo le informazioni necessarie per proseguire
    support_t* supportPTR = (support_t*)SYSCALL(GETSUPPORTPTR,0,0,0); // ottengo il puntatore alla struttura di supporto del processo corrente
    int cause = supportPTR->sup_exceptState[PGFAULTEXCEPT].cause; //ottengo l'eccezione
    cause = cause & CAUSE_EXCCODE_MASK; //estraggo dalla cause il valore che indica se pagefault o TLBmod
    if(cause == EXC_MOD){ //causa modifica tlb: queste costanti si trovano in /uriscv/cpu.h. Inoltre questa eccezione non si dovrebbe mai verificare perchè abbiamo DIRTYON sempre attivo
        trapHandler(supportPTR); 
    } 

    //gain del mutual access alla swap pool
    SYSCALL(PASSEREN,(int)&swapPoolSemaphore,0,0);  //in headers/const.h

    //determino la missing page
    unsigned int entryHi = supportPTR->sup_exceptState[PGFAULTEXCEPT].entry_hi;
    int missingVpn = (entryHi &(GETSHAREFLAG | GETPAGENO)) >> VPNSHIFT; // con getSHAREFLAG conservo tutti i bit che indicano la pagina: 0x80005000 -> 0x80005
    // dato un indirizzo 0x80005 o 0xBFFFFF controlla gli ultimi 8 bit: se 0-30 ritorna la pagina, altrimenti se FF = 255 ritorna pagina 31(stack)  
    int missingPage = vpnToPage(missingVpn);
    /*debug*/
    klog_print("--Missing Page--");
    klog_print_dec(missingPage); // scrive sul buffer al contrario quando metti in ascii
    klog_print("--End--");
    /*--*/
    int isFree = 0; //per distinguere se fare o no punto 8: 1-> esiste un frame non ancora occupato, 0 tutti i frame sono occupati

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
    //punto 8-> solo se frame occupato. Update atomico pageT e TLB: invalido la entry victim e poi write nel backing store.
    if(isFree != 1){
        atomicRefresh(&swapPoolTable[frameVictim],-1,0); // --> Invalido le entry in tlb e pageTable
         /* Ora devo scrivere sul device DATA0 field con il corretto indirizzo di start del blocco da 4k: Il frameStartAddress*/
        int killedPage = vpnToPage(swapPoolTable[frameVictim].sw_pageNo);
        int IOstatus =rwToMem(frameVictim,swapPoolTable[frameVictim].sw_asid,killedPage,1);//scrivo la pagina da killare in memoria.
        if(IOstatus != READY){ //operazione non andata bene
            SYSCALL(VERHOGEN,(int)&swapPoolSemaphore,0,0);
            trapHandler(supportPTR);
        }
    }
    //leggo da flashdev nel frame
    int IOstatus = rwToMem(frameVictim,supportPTR->sup_asid,missingPage,2);
    if(IOstatus != READY){ //operazione non andata bene
            SYSCALL(VERHOGEN,(int)&swapPoolSemaphore,0,0);
            trapHandler(supportPTR);
    }
    //punto 10: Aggiorno la entry della swapPoolTable
    swapPoolTable[frameVictim].sw_asid = supportPTR->sup_asid;
    swapPoolTable[frameVictim].sw_pageNo = missingVpn;
    swapPoolTable[frameVictim].sw_pte = &supportPTR->sup_privatePgTbl[missingPage];
    //punto 11/12 atomic update, Valid on e aggiornare anche PFN corretto
    atomicRefresh(&swapPoolTable[frameVictim],frameAddr,1);   
    //step 13
    SYSCALL(VERHOGEN,(int)&swapPoolSemaphore,0,0);
    //step 14
    LDST(&supportPTR->sup_exceptState[PGFAULTEXCEPT]);
}

// Parametro validation = 0 invalida la pagina, validation = 1 valida la pagina. frame serve per aggiornare correttamente la PFN
void atomicRefresh(swap_t* swapFrame,int frame,int validation){
    //salvo il "vecchio" stato
    unsigned int status = getSTATUS();
    //disabilito interrupt
    setSTATUS(getSTATUS() & ~MSTATUS_MIE_MASK);
    
    if(validation == 0){ // devo invalidare
        //devo individuare la pagina relativa da invalidare: swa_pte PTE==Page Table Entry Devo solo modificare il bit V VALIDON lo faccio con and e ~VALIDO
        swapFrame->sw_pte->pte_entryLO &= ~VALIDON;
    }else if(validation == 1){ //devo validare
        //questo funziona perchè frame , DIRTYON e VALIDON occupano bit diversi. frame è allineato con PAGESIZE(4096 o 0x1000) quindi ultimi 12bit sono vuoti dove stanno i flag
        swapFrame->sw_pte->pte_entryLO = frame | DIRTYON | VALIDON;
    }
    //controllo la entry nel tlb
    setENTRYHI(swapFrame->sw_pte->pte_entryHI);
    TLBP();
    if((getINDEX() & PRESENTFLAG)==0){ //il registro index della CPU0 ha il most sig. bit = P, e la entry della tlb in 13-8 bit. uso la maschera per estrarre il bit P.
        setENTRYLO(swapFrame->sw_pte->pte_entryLO);
        TLBWI();
    }

    //riattivo interrupts
    setSTATUS(status);
    
}
/**in scrittura da ram a bs: prima invalido pag poi scrivo in bs
 *in lettura da bs a ram: prima copio pagina nel frame(che al momento contiene una pagina V=0) poi modifico V = 1.
 *si dice write/read in relazione all'operazione dal backingstore: asid 1-8 e flash 0-7
 *op = 1 -> write 2->read, altri valori -> PANIC()
 *devAddrBase = START_DEVREG+ ((IntlineNo - 3) * 0x80)+ (DevNo * 0x10); Formula di phase2, intLineNo è 4(si trova in interrupts.c)
 * Deve ritornare lo status dell'op.
 * Op = 1 write Op = 2 read
 * nel command devo scrivere
**/
int rwToMem(int frameVictim,int asid,int page,int op){
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
    //nel comando dico pagina su cui operare e operazione
    if(op == 1){//write
        command = (page << 8) | FLASHWRITE; //in command least sig. byte è il comando: 8 equivale a lasciare libero un byte(quello del comando): 
    }else if(op == 2){ //read
        command = (page << 8)  | FLASHREAD;
    }
    /*debug*/
    klog_print("--ASID--");
    klog_print_dec(asid);
    klog_print("----");

    klog_print("--DEV--");
    klog_print_dec(devNumber);
    klog_print("----");

    klog_print("--COMMAND ADDR--");
    klog_print_hex((unsigned int)commandAddr);
    klog_print("----\n");
    /*--*/
    //dopo aver caricato i dati corretti dico al kernel di eseguire la DOIO
    int ioStatus =SYSCALL(DOIO,(int)commandAddr,command,0);
    return ioStatus;
}



//pager: ottiene la pagina che vuole essere caricata, cerca se c'è un frame libero, se sì la carica e basta, altrimenti: deve selezionare il frame da killare, al suo interno c'è la pagina toKill
// ora dobbiamo: invalidare la pagina, scrivere toKill nel flashDevice,scrivere la pagina nuova toAdd nel frame, aggiornare i dati della swapPoolTable, aggiornare le tabelle. 


//converte il vpn in pagina

int vpnToPage(int vpn){
    unsigned int pageFlag = 0xff;
    vpn = vpn & pageFlag;
    if(vpn == 255)
        return 31;
    else if(vpn >= 0 && vpn <= 30)
        return vpn;
    else{
        PANIC(); //per il momento così
        return -1;
    }
        
}

// Devo invalidare le entry: prendo il mutex, scorro la swapPoolTable e devo invalidare pageEntry corrispondente ed eventualmente il TLB.
void freeFrames(int asid){
    SYSCALL(PASSEREN,(int)&swapPoolSemaphore,0,0);
    for(int i = 0 ; i < POOLSIZE;i++){
        if(swapPoolTable[i].sw_asid == asid){
            unsigned int status = getSTATUS();
            setSTATUS(getSTATUS() & ~MSTATUS_MIE_MASK); // interrupts disabilitati
            swapPoolTable[i].sw_pte->pte_entryLO &= ~VALIDON; // invalido la entry
            setENTRYHI(swapPoolTable[i].sw_pte->pte_entryHI);
            TLBP();
            if((getINDEX() & PRESENTFLAG)==0){ //il registro index della CPU0 ha il most sig. bit = P, e la entry della tlb in 13-8 bit. uso la maschera per estrarre il bit P.
                setENTRYLO(swapPoolTable[i].sw_pte->pte_entryLO);
                TLBWI();
            }
            setSTATUS(status);
            swapPoolTable[i].sw_asid = -1;
            swapPoolTable[i].sw_pageNo = -1;
            swapPoolTable[i].sw_pte = NULL; 
        }
    
    }
    SYSCALL(VERHOGEN,(int)&swapPoolSemaphore,0,0);
}