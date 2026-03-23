#include "kernel.h"

void scheduler(){


klog_print(" empty=");
klog_print_hex(emptyProcQ(&ready_queue));
klog_print(" proc=");
klog_print_hex(process_counter);
klog_print(" soft=");
klog_print_hex(soft_block_counter);
klog_print(" curr=");
klog_print_hex((unsigned int)current_process);

  if (!emptyProcQ(&ready_queue)){

  current_process = removeProcQ(&ready_queue); // rimuovo il PCB dalla testa dei ready queue e lo metto come processo corrente (inizio a eseguire il processo)
 
  setTIMER(TIMESLICE);
  STCK(slice_start); // legge il tempo corrente del clock
  LDST(&current_process->p_s);
  
  }
  if (process_counter == 0){ 
   HALT();
   }  
  if (process_counter >0 && soft_block_counter >0){
    setMIE(MIE_ALL  & ~MIE_MTIE_MASK);
    unsigned int status = getSTATUS();
    status |= MSTATUS_MIE_MASK;
    setSTATUS(status);
    WAIT();

  }else if (process_counter >0 && soft_block_counter ==  0){
    klog_print("Panico scheduler: deadlock");
    bp();
    PANIC();
  }
 
}
