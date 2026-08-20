#ifndef SUPPORT_LV4_H
#define SUPPORT_LV4_H

#include "../headers/types.h"
#include "../headers/const.h"
#include <uriscv/types.h>
#include <uriscv/liburiscv.h>
#include <uriscv/arch.h>
#include <uriscv/cpu.h>

#define READTERMINAL 5
#define EXECUTE 6
#define SWAP_POOL_START 0x20020000
      
/*semafori per garantire che processo padre non lasci orfani i figli */
extern int masterSemaphore; //semaforo del primo processo: viene fatta V solo quando la shell lanciata termina init 0
extern int shellSemaphore; //semaforo di tutti i processi lanciati dalla shell. viene fatta la V solo quando l'ultimo processo lanciato muore. init 0
extern int swapPoolSemaphore;// semaforo per garantire accesso esclusivo all'area di swap pool

/*semafori per lettura/scrittura shell*/
extern int readTermsemaphore;
extern int writeTermsemaphore;




/*funzioni generali*/
//inizializzazione test
void test();
//inizializzazione strutture condivise(swap pool table)
void initSwapTable();
//creazione U-proc
void processCreation(int asid);
//pager
void pager();
//general exception handler
void generalExceptionHandler();
//trapHandler
void trapHandler(support_t*spt);
//syscall handler
void UsyscallHandler(support_t*spt);

void freeFrames(int asid);



extern void klog_print(char *msg);
extern void klog_print_dec(unsigned int num);
extern void klog_print_hex(unsigned int num);
#endif