#include <uriscv/const.h>
#include <uriscv/types.h>

/** tlb refill da fare o no? presente già in p2test */
void exception_handler();
void tlb_refill_handler();
void syscallHandler();

void exception_handler(){
    int cpu_id = getPRID(); // otteniamo l'id della cpu corrente
    state_t* ptr_state = GET_EXCEPTION_STATE_PTR(cpu_id); // puntatore a state_t

    int cause_code = getCAUSE();
    if(CAUSE_IS_INT(cause_code))
    {
        interruptHandler();
    }
    else if(cause_code >= 24 && cause_code <= 28){
        tlbHandler(); 
    }
    else if(cause_code == 8 || cause_code == 11 ){ // dobbiamo controllare se kernel mode processore(?)
        syscallHandler();
    }
    else{ // se sono solo 28 i restanti rientrano qui
        trapHandler();
    }

}


void syscallHandler(){
    /**
     * Controllo registri a0-a3 per individuare il valore della syscall
     * a0 < 0 e processore in kernel mode
     * NSYS1-NSY10 in User-Mode allora traphandler. attempt to request a non-existent Nucleus service should trigger a Program Trap exception too.
     * In particular the Nucleus should simulate a Program Trap exception when a privileged service
     * is requested in user-mode. This is done by setting the cause field in the stored exception state to
     * PRIVINSTR (Privileged Instruction), and calling one’s Program Trap exception handler.
     */
}
