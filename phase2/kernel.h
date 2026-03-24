#ifndef KERNEL_H
#define KERNEL_H

/* Inclusione delle librerie usate in questa fase */
#include "../phase1/headers/asl.h" // initASL
#include "../phase1/headers/pcb.h" // initQueue

#include "../headers/listx.h" // per poter usare list_head
#include "../headers/const.h" // per poter usare le costanti al posto degli indirizzi SEMDEVLEN,PASSUPVECTOR
#include "../headers/types.h" // per poter usare pcb_t

#include <uriscv/const.h>
#include <uriscv/types.h>
#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h>
#include <uriscv/arch.h>
/* Dichiarazione delle variabili globali*/

/*
 * Process count -> int
 * Soft-block count -> int, numero di processi partiti ma non ancora terminati
 * Ready queue -> coda di pcb in ready state quindi direi una list_head
 * Current Process -> pcb_t* puntatore al processo attualmente in running state
 * Device semaphore -> il kernel tiene un intero per ogni device esterno più uno per lo pseudo-clock. (Dal momento che i terminali sono due device indipendenti,
 * il kernel tiene due semafori per ogni terminale.): ogni device è associato ad un intero, quando un processo richiede un'op
 * i/o viene spostato da running alla coda relativa all'integer semaforo del dispositivo a cui viene fatta la richiesta e fatta al P.
 * Quando il dispositivo ha finito, invia l'interrupt e il processore quando riesce fa la V spostando il processo bloccato di nuovo nella coda.
 */

/* qui variabili vengono "promesse" al linker*/
#define PSEUDO_CLOCK_SEM_INDEX (SEMDEVLEN - 1) //indice del semaforo pseudo-clock
#define DEVREGBASE 0x10000054
extern int process_counter; //Quanti processi attualmente presenti
extern int soft_block_counter; //Quanti processi "Blocked" (ASL)
extern struct list_head ready_queue; // coda dei processi
extern pcb_t* current_process;
extern int device_sem [SEMDEVLEN]; // un sem per device + 1 per pseudo-clock
extern int* pseudo_clock_sem; // questo indirizzo sarà solo per lo pseudoclock
extern cpu_t slice_start; // momento di inizio esecuzione processo
/** funzioni */
void scheduler();

int* sem_index_from_dev(int IntlineNo, int devNo,memaddr inneroffset);
void interruptHandler(state_t* ptr_exc);
void uTLB_RefillHandler();

/** Klog_prints e bp */
extern void klog_print(char *msg);
extern void klog_print_dec(unsigned int num);
extern void klog_print_hex(unsigned int num);
void bp();

#endif 