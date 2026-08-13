/*eccezioni generali e syscall utente*/
#include "supportL.h"
//GeneralExceptionHandler gestisce i casi: Syscall o trap
void generalExceptionHandler();
//trapHandler
/** trapHandler chiama direttamente Syscall2.*/
void trapHandler(support_t*spt);
//syscall handler
void UsyscallHandler(support_t *sup);

void Syscall2(support_t* sup);



void generalExceptionHandler(){
    // devo ottenere il tipo di eccezione ed indirizzarla nell'handler corretto: syscall o trap
    support_t* supportPtr = (support_t*) SYSCALL(GETSUPPORTPTR,0,0,0);
    //devo comprendere la cause: copio il codice di fase2
    unsigned int cause = supportPtr->sup_exceptState[GENERALEXCEPT].cause;
    unsigned int cause_code = cause & CAUSE_EXCCODE_MASK; // converte il cause in un valore "comprensibile"
    if(cause_code == 8 || cause_code == 11){ // nel file di phase2 dice che una eccezione con cause 8 o 11 è da passare al piano superiore
        UsyscallHandler(supportPtr);
    }else{
        trapHandler(supportPtr);
    }

}


void UsyscallHandler(support_t *sup){
    state_t* stato = &sup->sup_exceptState[GENERALEXCEPT];
    int num = stato->reg_a0;
    stato->pc_epc +=WORDLEN;
    switch(num){
        case (TERMINATE):
            Syscall2(sup);
            break;
        /*case (WRITETERMINAL):
            Syscall4(sup);
            break;
        case (READTERMINAL):
            Syscall5(sup);
            break;
        case (EXECUTE):
            Syscall6(sup);
            break;*/
        default:
            Syscall2(sup); // default terminiamo perchè c'è stato un errore.
            break;
    }
    LDST(&sup->sup_exceptContext[GENERALEXCEPT]);
}

//terminate: quello che fa è pulire le strutture dati del processo che la invoca, se il processo è la shell fa una V su mastersem altrimenti fa la V su shellsem
/**bisogna ancora fare in modo che si puliscano le strutture necessarie:
 * SwapPoolTable e TLB
 * sup_asid
 * pageTable -> invalidare
 **/
void Syscall2(support_t* sup){
    if(sup->sup_asid == 1){// il valore che ho scelto come asid della shell
        SYSCALL(VERHOGEN,(int)&masterSemaphore,0,0);
    }else{
        SYSCALL(VERHOGEN,(int)&shellSemaphore,0,0);
    }
    SYSCALL(TERMPROCESS,0,0,0);
    } 
/*
void SyScall4(support_t* sup){


}

void SyScall5(support_t* sup){

}

void SyScall6(support_t* sup){

}
*/
void trapHandler(support_t* sup){
    Syscall2(sup);
}
