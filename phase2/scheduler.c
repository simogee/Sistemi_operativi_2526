#include "kernel.h"

extern void bp();
void scheduler(){
  if (!emptyProcQ(&ready_queue)){

  current_process = removeProcQ(&ready_queue); // rimuovo il PCB dalla testa dei ready queue e lo metto come processo corrente (inizio a eseguire il processo)
  process_counter--;
  setTIMER(TIMESLICE);
  // klog_print("woooo");
    bp();
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
    PANIC();
  }
 
}
