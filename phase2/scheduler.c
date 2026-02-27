#include "../phase1/headers/asl.h" // initASL
#include "../phase1/headers/pcb.h" // initQueue

#include "../headers/listx.h" // per poter usare list_head
#include "../headers/const.h" // per poter usare le costanti al posto degli indirizzi SEMDEVLEN,PASSUPVECTOR
#include "../headers/types.h" // per poter usare pcb_t

#include <uriscv/const.h>
#include <uriscv/types.h>
#include <uriscv/liburiscv.h>
extern struct list_head ready_queue;
extern pcb_t* current_process;
extern int process_counter; //Quanti processi attualmente presenti
extern int soft_block_counter; //Quanti processi "Blocked" (ASL)
void scheduler(){
  while (TRUE){
    if (process_counter == 0) HALT();
    if (process_counter >0 && soft_block_counter >0){
      setMIE(MIE_ALL  & ~MIE_MTIE_MASK);
      unsigned int status = getSTATUS();
      status |= MSTATUS_MIE_MASK;
      setSTATUS(status);
      WAIT();
    }
    if (process_counter >0 && soft_block_counter ==  0){
      PANIC();
    }


    current_process = removeProcQ(&ready_queue); // rimuovo il PCB dalla testa dei ready queue e lo metto come processo corrente (inizio a eseguire il processo)
    setTIMER(TIMESLICE);
    LDST(&current_process->p_s);
  }
}
