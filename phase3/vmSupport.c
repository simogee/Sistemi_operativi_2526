/* dove implementiamo swap pool table, pager,I/O flash device*/
#include "supportL.h"
#include "../phase2/kernel.h" // devo includerlo per permettere a uTLBrefillHandler di accedere alle strutture dati globali di fase 2

//metto l'indirizzo suggerito sulle specifiche:
#define SWAP_POOL_START 0x20020000
swap_t swapPoolTable[POOLSIZE];

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
    for(int i = 0; i < POOLSIZE; i++){
        swapPoolTable[i].sw_asid   = -1;
        swapPoolTable[i].sw_pageNo = -1; // ci metto un valore impossibile(sperem)
        swapPoolTable[i].sw_pte    = NULL;
    }

}
/** Dell'associazione: entrySwapPoolTable e frame se ne occuperà il pager, questa è una mini funzione che userà per associare gli indici(swpt) con i frame. */
unsigned int addressSwapPool(int i){
    if(i < 0 || i >= POOLSIZE){ // non dovremme mai verificarsi
        PANIC();
    }
    return SWAP_POOL_START +(i*PAGESIZE);
}
//funzioni I/O: lettura da device into mem e viceversa
void writeIntoDev();
void readIntoMem();
//pager
/** 
 * 1:
 *  Deve ottenere support_t* con NSYS8
 *  Leggere la cause
 *  Distinguere TLBMod da pageFault
 *  Ottenere la VPN mancante
 *  Prendere mutual access della swap pool
 *  
 * 2:
 *  Implementare fifo/roundRobin
 *  Ottenere indice i(pagina vittima)
 *  Calcolare il frame con addressSwapPool
 *  
 * 3(caso frame occupato):
 *  Invalidare entry vecchia in PageTable e aggiornare TLB(Atomico)
 *  scrivere il vecchio frame nel backing store
 * 4:
 *  Leggere la backing store nel frame
 *  Aggiornare swapPoolTable[i]
 *  Aggiornare PFN e V=1 in pageTable e TLB(atomico)
 * 5:
 *  Rilasciare mutex e fare LDST senza incrementare il pc.
 * -------------------------------------------------------
 * Il pager viene chiamato ogni qual volta ci sia bisono di aggiornare le pagine in pageTable(caricamento da Backing store e scrittura in Backing Store)
 * Questo rende fifoPages ottimo per ciclare tra le pagine: inizia che sceglie il frame 0 che ospita la pagina caricata da più tempo e così via.
 */
void pager(){

    //prendo le informazioni necessarie per proseguire
    support_t* supportP = (support_t*)SYSCALL(GETSUPPORTPTR,0,0,0); // ottengo il puntatore alla struttura di supporto
    int cause = supportP->sup_exceptState[PGFAULTEXCEPT].cause; //ottengo l'eccezione
    cause = cause & CAUSE_EXCCODE_MASK; //estraggo dalla cause il valore che indica se pagefault o TLBmod
    if(cause == EXC_MOD){ //causa modifica tlb: queste costanti si trovano in /uriscv/cpu.h
        trapHandler(supportP); //skrr
    } 

    //gain del mutual access alla swap pool
    SYSCALL(PASSEREN,&swapPoolSemaphore,0,0);  //TBH ho provato a metterci un nome e ha funzionato. non ho ancora capito dove sia la def di passaren

    //determino la missing page(Forse si può direttamente fare una funzione poichè non è la prima volta che mi viene chiesto)
    unsigned int entryHi = supportP->sup_exceptState[PGFAULTEXCEPT].entry_hi;
    int vpn = (entryHi & GETPAGENO) >> VPNSHIFT;


    //devo trovare un frame da liberare: caso 1. esiste un frame vuoto, caso 2 devo eliminare una pagina
    int framevictim = -1;
    for(int i = 0; i < POOLSIZE;i++){
        if(swapPoolTable[i].sw_asid == -1){
            framevictim = i;
            break;
        }      
    }
    // non è stato trovato un frame libero
    if(framevictim == -1){ 
        framevictim = fifoPages % POOLSIZE;
        fifoPages++;
    } 
    //Punto 8 todo
    /**
     * 
    (A)Update process x’s Page Table: mark Page Table entry k as not valid. This entry is easily
    accessible, since the Swap Pool table’s entry i contains a pointer to this Page Table entry.
    (b) Update the TLB, if needed. The TLB is a cache of the most recently executed process’s
    Page Table entries. If process x’s page k’s Page Table entry is currently cached in the TLB
    it is clearly out of date; it was just updated in the previous step.
    Important: This step and the previous step must be accomplished atomically [Section 5.3].
    (c) Update process x’s backing store. Write the contents of frame i to the correct location on
    process x’s backing store/flash device [Section 5.1]. Treat any error status from the write
    operation as a program trap [Section 8].
      */


    
  
}

