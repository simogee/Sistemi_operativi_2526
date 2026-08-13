/*eccezioni generali e syscall utente*/
#include "supportL.h"
//GeneralExceptionHandler gestisce i casi: Syscall o trap
void *generalExceptionHandler();
//trapHandler
/** trapHandler chiama direttamente sys2.*/
void trapHandler(support_t*spt);
//syscall handler
void UsyscallHandler(int num);




void *generalExceptionHandler(int num){
    switch(num){
        case
    }
   
}


void UsyscallHandler(int num){

}

//terminate: quello che fa è pulire le strutture dati del processo che la invoca, se il processo è la shell fa una V su mastersem altrimenti fa la V su shellsem
/**bisogna ancora fare in modo che si puliscano le strutture necessarie:
 * SwapPoolTable e TLB
 * sup_asid
 * pageTable -> invalidare
 **/
void Sys2(support_t* sup){
    if(sup->sup_asid == 1){// il valore che ho scelto come asid della shell
        SYSCALL(VERHOGEN,(int)&masterSemaphore,0,0);
    }else{
        SYSCALL(VERHOGEN,(int)&shellSemaphore,0,0);
    }
    SYSCALL(TERMINATE,0,0,0);
    } 

void trapHandler(support_t* sup){
    Sys2(sup);
}