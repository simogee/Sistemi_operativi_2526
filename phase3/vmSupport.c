/* dove implementiamo swap pool table, pager,I/O flash device*/
#include "supportL.h"
swap_t swapPoolTable[POOLSIZE];

/*funzioni da implementare*/
// inizializzazione della swap pool table
void initSwapTable(){}
//funzioni I/O: lettura da device into mem e viceversa
void writeIntoDev();
void readIntoMem();
//pager
void pager();