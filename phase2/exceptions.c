#include <uriscv/const.h>
#include <uriscv/types.h>
#include "kernel.h"
/** tlb refill da fare o no? presente già in p2test */
void exception_handler();
void syscallHandler();
void uTLB_RefillHandler();

state_t* ptr_exc; //lo dichiaro qui così posso utilizzarlo liberamente nelle varie funzioni

void uTLB_RefillHandler() {
setENTRYHI(0x80000000);
setENTRYLO(0x00000000);
TLBWR();
LDST((state_t*) BIOSDATAPAGE);
}



void exception_handler(){

    ptr_exc = (state_t*) BIOSDATAPAGE; // puntiamo al bios_datapage per poter estrarre i valori dei campi necessari alla corretta gestione dell'exception
    unsigned int cause = getCAUSE();
   
    if(CAUSE_IS_INT(cause))   
    {
        interruptHandler(); //questo sarà in un altro file.
    }
    else{
        unsigned int cause_code = cause & CAUSE_EXCCODE_MASK; //valore del registro cause e con la maschera CAUSE_EXCCODE_MASK ritorniamo il codice dell'eccezione
        if (cause_code == 8 || cause_code == 11)
            syscallHandler();
        else if (cause_code >= 24 && cause_code <= 28)
            tlbHandler();
        else
            trapHandler();
    }

}


void syscallHandler(){
    /**
     * Controllo registri a0-a3 per individuare il valore della syscall
     *
     * NSYS1-NSY10 in User-Mode allora traphandler. attempt to request a non-existent Nucleus service should trigger a Program Trap exception too.
     * In particular the Nucleus should simulate a Program Trap exception when a privileged service
     * is requested in user-mode. This is done by setting the cause field in the stored exception state to
     * PRIVINSTR (Privileged Instruction), and calling one’s Program Trap exception handler.
     */


     /** prima cosa: check del registro a0 < 0 e mode-> kernel allora eseguo NSYS1-NSY10. se sono richiesti in usermode -> trapHandler
      *  la definizione di state_t sta in uriscv/types.h
      * MPP= machine previous privilege 
      * MSTATUS_MPP_U = user mode 
      * MSTATUS_MPP_M = kernel mode
      */
     if(ptr_exc->reg_a0 < 0 && (ptr_exc->status & MSTATUS_MPP_MASK) == MSTATUS_MPP_M){
        //qui dobbiamo sviluppare le nostre syscall NSYS1-NSY10
        // dentro const.h degli header locali abbiamo le def per le syscalls
        // bloccanti: (NSYS3, NSYS5, NSYS7 and NSYS10)
        switch(a0){
            case CREATEPROCESS:
            case TERMPROCESS:
            case PASSEREN:
            case VERHOGEN:
            case DOIO:
            case GETTIME:
            case CLOCKWAIT:
            case GETSUPPORTPTR:
            case GETPROCESSID:
            case YIELD:
            default:
                trapHandler();
        }
     }
     else if(ptr_exc->reg_a0 < 0 && (ptr_exc->status & MSTATUS_MPP_MASK) == MSTATUS_MPP_U){
        ptr_exc->status = PRIVINSTR; // errore di permesso
        trapHandler();
     }
     else{ //richiesta insesistente
        trapHandler();
     }
}





