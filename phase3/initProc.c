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

int flashSemaphore[]; //??

int readTerm;
int writeTerm;