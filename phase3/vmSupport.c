/* dove implementiamo swap pool table, pager,I/O flash device*/
#include "supportL.h"
#include "../phase2/kernel.h"

//metto l'indirizzo suggerito sulle specifiche:
#define SWAP_POOL_START 0x20020000
/** Piccolo offtopic: pagine sono da 4096 byte. quindi in hex: 0x1000. Dobbiamo lasciare spazio per 32 pagine che in hex:0x0002. quindi in totale 0x2000 che aggiunto all'indirizzo start 0x20000000 diventa 0x20020000 */
swap_t swapPoolTable[POOLSIZE];

int swapPoolSemaphore;
//spostata da phase2 a phase3. Però accede ugualmente a dati di phase2 quindi ho importato kernel.h
void uTLB_RefillHandler(){
    
 }
/*funzioni da implementare*/
// inizializzazione della swap pool table
void initSwapTable(){
    for(int i = 0; i < POOLSIZE; i++){
        swapPoolTable[i].sw_asid = -1;
        swapPoolTable[i].sw_pageNo=0;
        swapPoolTable[i].sw_pte=0;
    }

}
//funzioni I/O: lettura da device into mem e viceversa
void writeIntoDev();
void readIntoMem();
//pager
void pager(){}