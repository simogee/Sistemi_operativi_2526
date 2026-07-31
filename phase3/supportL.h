#ifndef SUPPORT_LV4_H
#define SUPPORT_LV4_H

#include "../headers/types.h"
#include "../headers/const.h"
/*semafori per garantire che processo padre non lasci orfani i figli */
extern int masterSemaphore; 
extern int shellSemaphore;

/*semaforo per garantire mutua esclusione dei flash device*/
extern int flashSemaphore[];

/*semafori per lettura/scrittura shell*/
extern int readTerm;
extern int writeTerm;



/*funzioni generali*/
//inizializzazione strutture condivise(swap pool table)
void initSwapPoolTable();
//creazione U-proc
//pager
//general exception handler
//program trap handler

#endif