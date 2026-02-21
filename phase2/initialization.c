#include <../phase1/headers/asl.h> // initASL
#include <../phase1/headers/pcb.h> // initQueue

#include <../headers/listx.h> // per poter usare list_head
#include <../headers/const.h> // per poter usare le costanti al posto degli indirizzi SEMDEVLEN,PASSUPVECTOR
#include <../headers/types.h> // per poter usare pcb_t 


extern void uTLB_RefillHandler(), test(),exception_handler(); // funzioni provided esternamente e execption va ancora creata
const int PSEUDO_CLOCK_SEM_INDEX = SEMDEVLEN -1; // indirizzo fisso per lo pseudo-clock
/**
 * 
 * Variabili da dichiarare:
 * Process count -> uint
 * Soft-block count -> uint, numero di processi partiti ma non ancora terminati
 * Ready queue -> coda di pcb in ready state quindi direi una list_head
 * Current Process -> pcb_t* puntatore al processo attualmente in running state
 * Device semaphore -> il kernel tiene un intero per ogni device esterno più uno per lo pseudo-clock. (Dal momento che i terminali sono due device indipendenti,
 * il kernel tiene due semafori per ogni terminale.): ogni device è associato ad un intero, quando un processo richiede un'op
 * i/o viene spostato da running alla coda relativa all'integer semaforo del dispositivo a cui viene fatta la richiesta e fatta al P.
 * Quando il dispositivo ha finito, invia l'interrupt e il processore quando riesce fa la V spostando il processo bloccato di nuovo nella coda.
 * 
 * 
 *   
 * Bisogna inizializzare il Pass-Up Vector -> il passup vector è un vettore i cui campi puntano alle funzioni interrupt handlers (indirizzo: 0x0FFFF900)
 * 
 * 
 * Inizializzare le strutture dati della phase1, includere quindi i file della fase precedente
 * 
 * Inizializzare le strutture dati e variabili phase2 initPcbs() e initASL()
 * 
 * Load the system-wide interval Timer con 100 ms -> no idea atm.
 * 
 * Istanziare un process test
 * 
 * 
 * Chiamare lo scheduler
 * 
 */

 /*init variabili */
 int process_counter; //Quanti processi attualmente presenti
 int soft_block_counter; //Quanti processi "Blocked" (ASL)
 struct list_head ready_queue; // coda dei processi 
 pcb_t* current_process;
 int device_sem [SEMDEVLEN]; // un sem per device + 1 per pseudo-clock
 int* pseudo_clock_sem = &device_sem[PSEUDO_CLOCK_SEM_INDEX]; // questo indirizzo sarà solo per lo pseudoclock

int main(){

/* inizializzazione del pass-up Vector la struttura passupvector_t si trova in usr/include/uriscv */
passupvector_t* pass_up_vector       = (passupvector_t*) PASSUPVECTOR ;
 pass_up_vector->tlb_refill_handler   = (memaddr) uTLB_RefillHandler;
 pass_up_vector->tlb_refill_stackPtr = (memaddr) KERNELSTACK; // top della funzione
 pass_up_vector->exception_handler   = (memaddr) exception_handler;
 pass_up_vector->exception_stackPtr  = (memaddr) KERNELSTACK;

 /*inizializzo strutture phase1*/
 initASL();
 initPCBs();

 process_counter = 0;
 soft_block_counter = 0;
 mkEmptyProcQ(&ready_queue);
 current_process = NULL;

 //inizializzo tutti i sem a 0
 for(int i = 0; i < SEMDEVLEN){
    device_sem[i] = 0;
 }
LDIT(PSECOND); //

}

