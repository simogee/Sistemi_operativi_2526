/* dove implementiamo swap pool table, pager,I/O flash device*/
#include "supportL.h"
#include "../phase2/kernel.h" // devo includerlo per permettere a uTLBrefillHandler di accedere alle strutture dati globali di fase 2

//metto l'indirizzo suggerito sulle specifiche:
#define SWAP_POOL_START 0x20020000
swap_t swapPoolTable[POOLSIZE];

int swapPoolSemaphore;
//spostata da phase2 a phase3. Però accede ugualmente a dati di phase2 quindi ho importato kernel.h

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
        swapPoolTable[i].sw_asid = -1;
        swapPoolTable[i].sw_pageNo=0;
        swapPoolTable[i].sw_pte=NULL;
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
void pager(){}

