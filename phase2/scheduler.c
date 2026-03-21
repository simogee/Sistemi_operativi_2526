#include "kernel.h"
extern void klog_print(char *msg);
extern void klog_print_dec(unsigned int num);
extern void klog_print_hex(unsigned int num);

void scheduler(){
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
