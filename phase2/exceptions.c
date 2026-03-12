#include <uriscv/const.h>
#include <uriscv/types.h>
#include "kernel.h"
/** tlb refill da fare o no? presente già in p2test */
void exception_handler();
void syscallHandler();
void uTLB_RefillHandler();


void uTLB_RefillHandler() {
setENTRYHI(0x80000000);
setENTRYLO(0x00000000);
TLBWR();
LDST((state_t*) BIOSDATAPAGE);
}



void exception_handler(){

    state_t* ptr_exc = (state_t*) BIOSDATAPAGE; // puntiamo al bios_datapage per poter estrarre i valori dei campi necessari alla corretta gestione dell'exception
    unsigned int cause = getCAUSE();

    if(CAUSE_IS_INT(cause))
    {
        interruptHandler(); //questo sarà in un altro file.
    }
    else{
        unsigned int cause_code = cause & CAUSE_EXCCODE_MASK; //valore del registro cause e con la maschera CAUSE_EXCCODE_MASK ritorniamo il codice dell'eccezione
        if (cause_code == 8 || cause_code == 11)
            syscallHandler(ptr_exc);
        else if (cause_code >= 24 && cause_code <= 28)
            tlbHandler();
        else
            trapHandler(); // no panic? 
    }

}


/** Per tutte le SYSTEMCALL vengono passati i parametri utilizzati nei registri a0-a3. a0 contiene il tipo di chiamata da effettuare*/
//il pid si incrementa con allocPcb() in automatico
void create_process(state_t* ptr_exc){
  pcb_t* new_proc = allocPcb(); // Nota: di defult:
                                // p_time = 0
                                // p_semAdd = NULL
                                // viene generato un val casuale al p_pid
  if (new_proc == NULL){ // non c'e spazio -> salvo -1 nel registro s0 del padre
    ptr_exc->reg_a0 = -1;
    return;
  }
  ptr_exc->reg_a0 = new_proc->p_pid; // c'e' spazio -> salvo il pid del figlio nel reg a0 del padre
  new_proc->p_s = *((state_t*) ptr_exc->reg_a1); // a1 (del padre) ha lo status di p_s del figlio: il padre deve preparare uno state_t da passare al figlio.
  if ((support_t*)ptr_exc->reg_a3 == NULL){
    new_proc->p_supportStruct = NULL;
  }else{
    new_proc->p_supportStruct = (support_t*) ptr_exc->reg_a3;
  }
  insertChild(current_process, new_proc);
  //nuovo processo va inserito nella testa della readyqueue
  insertProcQ(&ready_queue, new_proc);
  process_counter++; //questo forse non va ma va chiamato lo scheduler.
}

void terminate_process(state_t* ptr_exc){
// se PID = 0 elimino il current process (e i suoi figli)
if (ptr_exc->reg_a2 == 0){ 
  while(emptyChild(current_process)){
    pcb_t* child =removeChild(current_process);
    freePcb(child);
    process_counter--;
  }
  freePcb(current_process);
  process_counter--;
  return;
}

//se pid != 0 bisogna cercare il pcb.
pcb_t* process_to_rem = findByPid(ptr_exc->reg_a2);
process_to_rem = outProcQ(&ready_queue,process_to_rem); 
pcb_t* child_to_rem = NULL; // puntatore che useremo per rimuovere i child

// se lo troviamo sulla readyqueue
if(process_to_rem != NULL){
    while(emptyChild(process_to_rem)){
        child_to_rem = removeChild(process_to_rem);
        freePcb(child_to_rem);
        process_counter--;
    }
    freePcb(process_to_rem);   
    process_counter--;
    return; // vediamo poi i return come vanno gestiti
    }
// casi rimasti: è su un semaforo o il pid non esiste.
process_to_rem = outBlocked(process_to_rem); //check per vedere se è su un semaforo
if(process_to_rem != NULL){
    while (emptyChild(process_to_rem))
    {
       child_to_rem = removeChild(process_to_rem);
       freePcb(child_to_rem);
       process_counter--;
    }
    freePcb(process_to_rem);
    process_counter--;
    soft_block_counter--; //perchè era su un semaforo
    return;
}
// se arriviamo qui il pid non è valido
    return; //forse dovremmo gestirlo meglio

}


void Passeren(state_t* ptr_exc){
    //va fatta una p sull'address del semaforo che si trova in a1.
    //se > 0 allora decremento e controllo passato al current_process
    //se < 0 allora processo bloccato sul semaforo puntato e spostiamo il current_process sulla lista ASL relativa e si chiama scheduler.
}

void Verhogen(state_t* ptr_exc){
    //physical address sempre in a1
    //NON BLOCCANTE
    //se V diventa positiva ->  sveglio il pcb dal semaforo e lo metto in ready queue
    // se V resta negativa -> incremento e basta
    // poi riprende current 
}


void DoIO(state_t* ptr_exc){

}
void syscallHandler(state_t* ptr_exc){
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
        switch(ptr_exc->reg_a0){
            case CREATEPROCESS:
                create_process(ptr_exc);
                break;
            case TERMPROCESS:
                terminate_process();
                break;
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





/**
 * 
 */