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

#endif