#include <uriscv/const.h>
#include <uriscv/types.h>
#include "kernel.h"
/** tlb refill da fare o no? presente già in p2test */
void exception_handler();
void syscallHandler();
void uTLB_RefillHandler();


/*void uTLB_RefillHandler() {
setENTRYHI(0x80000000);
setENTRYLO(0x00000000);
TLBWR();
LDST((state_t*) BIOSDATAPAGE);
}*/

void subTree_killer(pcb_t* p);

void exception_handler(){

    state_t* ptr_exc = (state_t*) BIOSDATAPAGE; // puntiamo al bios_datapage per poter estrarre i valori dei campi necessari alla corretta gestione dell'exception
    unsigned int cause = getCAUSE();

    if(CAUSE_IS_INT(cause))
    {
        //interruptHandler(); //questo sarà in un altro file.
    }
    else{
        unsigned int cause_code = cause & CAUSE_EXCCODE_MASK; //valore del registro cause e con la maschera CAUSE_EXCCODE_MASK ritorniamo il codice dell'eccezione
        if (cause_code == 8 || cause_code == 11)
            syscallHandler(ptr_exc);
       /* else if (cause_code >= 24 && cause_code <= 28)
            //tlbHandler();
        else
            trapHandler(); // no panic? */
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




/** SYSTEMCALL per terminare i processi */
void terminate_process(state_t* ptr_exc){
// se ptr_exc->reg_a1 = 0 allora termino current_process
// altrimenti cerco il pid relativo.

pcb_t* process_to_kill = NULL;

if(ptr_exc->reg_a1 == 0){
    process_to_kill = current_process; 
}
else{
    process_to_kill = findByPid(ptr_exc->reg_a1); 
}
if(process_to_kill != NULL){ // esiste il processo da uccidere
    outChild(process_to_kill); // serve per rimuovere il processo dalla lista dei figli di un eventuale padre
    subTree_killer(process_to_kill);
}
    scheduler(); // termine di questa syscall richiama lo scheduler
}


void Passeren(state_t* ptr_exc){
    //va fatta una p sull'address del semaforo che si trova in a1((semAdd).
    //se >= 0 allora decremento e controllo passato al current_process
    //se < 0 allora processo bloccato sul semaforo puntato e spostiamo il current_process sulla lista ASL relativa e si chiama scheduler.
    /** Semaforo:
     *  p1->semadd = &device_sem[num]; 
     *  *p1->semadd = valore del semaforo; 
     */
    
    int* semaphore = (int*)ptr_exc->reg_a1;
    (*semaphore)--; //devo decrementare il valore puntato
    //funziona fino a qui
    if(*semaphore >= 0){ //NON bloccante
        //devo aggiornare il program counter e ritornare il controllo al current process
        ptr_exc->pc_epc+=WORDLEN;
        
        LDST(ptr_exc);
    }
    else{               //Bloccante

        ptr_exc->pc_epc += WORDLEN; // incremento il program counter di 4 per evitare di fare loop infinito
        current_process->p_s = *ptr_exc; // salvo lo stato aggiornato sul pcb
        cpu_t now;
        STCK(now);
        current_process->p_time += now - slice_start; //salviamo il empo passato dal dispatch del processo

        //devo inserire il processo nel semaforo
        int check = insertBlocked(semaphore,current_process); //inserisce il processo nella coda del semaforo relativo.
        if(check == 1){
            PANIC();  //non ci sono semafori liberi
        }
        soft_block_counter++;
        current_process = NULL; //dereferenzio il current process

        scheduler();
    }

}

void Verhogen(state_t* ptr_exc){
    //physical address sempre in a1
    //NON BLOCCANTE
    //se V diventa positiva ->  sveglio il pcb dal semaforo e lo metto in ready queue
    // se V resta negativa -> incremento e basta
    // poi riprende current 
    int* semaphore = (int*)ptr_exc->reg_a1;
    (*semaphore)++;
    if(*semaphore > 0){
    pcb_t* process_to_awake = removeBlocked(semaphore);
        if(process_to_awake != NULL){ // lo rimetto in readyqueue
            insertProcQ(&ready_queue,process_to_awake);
            soft_block_counter--;
        }
    }
    // non devo svegliare il processo
    ptr_exc->pc_epc+=WORDLEN;
    LDST(ptr_exc);
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
     //fino a qui funziona
     int syscallnum = (int) ptr_exc->reg_a0;
     if(syscallnum < 0 && (ptr_exc->status & MSTATUS_MPP_MASK) == MSTATUS_MPP_M){
        //qui dobbiamo sviluppare le nostre syscall NSYS1-NSY10
        // dentro const.h degli header locali abbiamo le def per le syscalls
        // bloccanti: (NSYS3, NSYS5, NSYS7 and NSYS10)
        switch(syscallnum){
            case CREATEPROCESS:
                create_process(ptr_exc);
                break;
            case TERMPROCESS:
                terminate_process(ptr_exc);
                break;
            case PASSEREN:
               //funziona fino a qui
                Passeren(ptr_exc);
                break;
            case VERHOGEN:
                Verhogen(ptr_exc);
                break;
            case DOIO:
            case GETTIME:
            case CLOCKWAIT:
            case GETSUPPORTPTR:
            case GETPROCESSID:
            case YIELD:
            default:
                //trapHandler();
        }
     }
     else if(syscallnum< 0 && (ptr_exc->status & MSTATUS_MPP_MASK) == MSTATUS_MPP_U){
        ptr_exc->cause = PRIVINSTR; // errore di permesso
        //trapHandler();
     }
     else{ //richiesta insesistente
        //trapHandler();
     }
}





/**
 * Funzione per rimuovere tutto il subtree dato un processo
 * Preso un processo: check figlio, se esiste richiamiamo subTree_killer su child ricorsivamente una volta che non esiste più un child:
 *      check se si trova su un semaforo:
 *              se si allora soft_block_counter --; e rimozione dal semaforo
 *      check se si trova sulla ready queue:
 *              rimozione dalla readyqueue;
 *      check se è il current_process:
 *              dereferenziamo current_process
 * 
 *      process_counter-- e liberiamo il pcb
 * freePcb(processo);     
 */




void subTree_killer(pcb_t* p){
    while(!emptyChild(p)){
        pcb_t* child = removeChild(p);
        subTree_killer(child);
    }
    if(p == current_process){
        current_process = NULL; // per dereferenziare il pcb_t*
    }
    else if(p->p_semAdd != NULL){ // si trova su un semaforo
        outBlocked(p);
        soft_block_counter--;
    }
    else{                        //non si trova su un semaforo check readyqueue 
        outProcQ(&ready_queue,p);
    }
    process_counter--;
    freePcb(p);
}
