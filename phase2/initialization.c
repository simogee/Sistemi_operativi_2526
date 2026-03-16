#include "kernel.h"
extern void uTLB_RefillHandler(), test(),exception_handler(),bp(),klog_print(char* str); // funzioni provided esternamente e execptionhandler andrà messa nel file exception
/**
 * Cosa fa initialization
 *
 *inizializza il Pass-Up Vector -> il passup vector è un vettore i cui campi puntano alle funzioni interrupt handlers (indirizzo: 0x0FFFF900)
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
/* qui vengono effettivamente definite, non in kernel.h*/
int process_counter; //Quanti processi attualmente presenti
int soft_block_counter; //Quanti processi "Blocked" (ASL)
struct list_head ready_queue; // coda dei processi
pcb_t* current_process;
int device_sem [SEMDEVLEN]; // un sem per device + 1 per pseudo-clock
int* pseudo_clock_sem = &device_sem[PSEUDO_CLOCK_SEM_INDEX]; // questo indirizzo sara+ solo per lo pseudoclock
cpu_t slice_start;
extern void test();

/**funzione da  gcc/libgcc/memcpy.c usata dal compilatore per copiare */
void *memcpy(void *dest, const void *src, unsigned int len)
{
	char *d = dest;
	const char *s = src;
	while (len--)
		*d++ = *s++;
	return dest;
}



int main(){

slice_start=0;
/* inizializzazione del pass-up Vector la struttura passupvector_t si trova in usr/include/uriscv */
passupvector_t* pass_up_vector       = (passupvector_t*) PASSUPVECTOR ; // * serve per poter accedere a PASSUPVECTOR
pass_up_vector->tlb_refill_handler   = (memaddr) uTLB_RefillHandler;
pass_up_vector->tlb_refill_stackPtr  = (memaddr) KERNELSTACK; // top della funzione
pass_up_vector->exception_handler    = (memaddr) exception_handler;
pass_up_vector->exception_stackPtr   = (memaddr) KERNELSTACK;

 /*inizializzo strutture phase1*/
 initASL();
 initPcbs();

 process_counter = 0;
 soft_block_counter = 0;
 mkEmptyProcQ(&ready_queue);
 current_process = NULL;

 //inizializzo tutti i sem a 0
 for(int i = 0; i < SEMDEVLEN;i++){
    device_sem[i] = 0;
 }
LDIT(PSECOND);

pcb_t* root = allocPcb(); //inizializza tutto a 0
RAMTOP(root->p_s.reg_sp); //stackpointer i registri sono definiti in uriscv/types.h grp[2]
root->p_s.status = MSTATUS_MPIE_MASK | MSTATUS_MPP_M; //enable interrupt
root->p_s.mie = MIE_ALL; //enable interrupt
root->p_s.pc_epc = (memaddr) test; //bisogna assegnare al pc del processo l'indirizzo della funzione test

//inizializzo Ready queue
INIT_LIST_HEAD(&ready_queue);

//metto root nella lista dei processi ready
insertProcQ(&ready_queue, root);
process_counter++;

scheduler(); //dobbiamo ancora fare


}


