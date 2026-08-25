/*eccezioni generali e syscall utente*/
#include "supportL.h"
//GeneralExceptionHandler gestisce i casi: Syscall o trap
void generalExceptionHandler();
//trapHandler
/** trapHandler chiama direttamente Syscall2.*/
void trapHandler(support_t*spt);
//syscall handler
void UsyscallHandler(support_t *sup);
//terminate
void Syscall2(support_t* sup);
//writeTerminal
int Syscall4(support_t*sup);
//readTerminal
void Syscall5(support_t*sup);
//Execute 
void Syscall6(support_t*sup);

int checkAddress(unsigned int address);



void generalExceptionHandler(){
    // devo ottenere il tipo di eccezione ed indirizzarla nell'handler corretto: syscall o trap
    support_t* supportPtr = (support_t*) SYSCALL(GETSUPPORTPTR,0,0,0);
    //devo comprendere la cause: copio il codice di fase2
    unsigned int cause = supportPtr->sup_exceptState[GENERALEXCEPT].cause;
    unsigned int cause_code = cause & CAUSE_EXCCODE_MASK; // converte il cause in un valore "comprensibile"
    /*debug*/
    klog_print("--Cause Code:--");
    klog_print_dec(cause_code);
    klog_print("--");
    /*--*/
    if(cause_code == 8 || cause_code == 11){ // nel file di phase2 dice che una eccezione con cause 8 o 11 è da passare al piano superiore
        UsyscallHandler(supportPtr);
    }else{
        trapHandler(supportPtr);
    }

}

// come nelle syscall di fase2 dobbiamo aumentare il pc una volta eseguita la syscall e caricare lo stato aggiornato
void UsyscallHandler(support_t *sup){
    state_t* stato = &sup->sup_exceptState[GENERALEXCEPT];
    int num = stato->reg_a0;
    stato->pc_epc +=WORDLEN;
    switch(num){
        case (TERMINATE):
            Syscall2(sup);
            break;
        case (WRITETERMINAL):
            Syscall4(sup);
            break;
        case (READTERMINAL):
            Syscall5(sup);
            break;
            
        case (EXECUTE):
            Syscall6(sup);
            break;
        default:
            Syscall2(sup); // default terminiamo perchè c'è stato un errore.
            break;
    }
    LDST(stato);
}

//terminate: quello che fa è pulire le strutture dati del processo che la invoca, se il processo è la shell fa una V su mastersem altrimenti fa la V su shellsem
/**bisogna ancora fare in modo che si puliscano le strutture necessarie:
 * SwapPoolTable e TLB
 * sup_asid
 * pageTable -> invalidare
 **/

void Syscall2(support_t* sup){
    freeFrames(sup->sup_asid);
    if(sup->sup_asid == 1){// il valore che ho scelto come asid della shell
        SYSCALL(VERHOGEN,(int)&masterSemaphore,0,0);
    }else{
        SYSCALL(VERHOGEN,(int)&shellSemaphore,0,0);
    }
    
    SYSCALL(TERMPROCESS,0,0,0);
    } 

void trapHandler(support_t* sup){
    /*debug*/
    klog_print("trap--entryhi--");
    klog_print_hex(sup->sup_exceptState[GENERALEXCEPT].entry_hi);
    klog_print("--");
    /*--*/
    Syscall2(sup);
}

/**WriteTerminal:
 * Deve sospendere il processo corrente, attendere che venga trasmessa una line di char
 * Passati: a0 = numero di syscall, a1 indirizzo di start,a2 lunghezza della stringa.
 * Se syscall termina con risultato != da "character trasmitted"(5) ritorna -1 in a0
 * Errori: richiedere di scrivere in un terminal dev fuori dallo scope di U-proc logical address space,
 *         a2 = 0
 *         a2 > 128
 * Ogni errore viene gestito con la chiamata di SYS2
 * Legge in una stringa il contenuto del terminale e ritorna la lunghezza del contenuto
 */
int Syscall4(support_t* sup){
    state_t* status = &sup->sup_exceptState[GENERALEXCEPT];
    int* a0                   = (int*)&status->reg_a0;
    unsigned int startingAddr = status->reg_a1;
    int length                = status->reg_a2;

    /*debug*/
    klog_print("len passata:  \n");
    klog_print_dec(length);
    klog_print("--\n");
    /*--*/
    
    // check dimensioni
    if(length < 0 || length > 128){
        *a0 = -1;
        /*debug*/
        klog_print("dim wrong");
        klog_print_dec(*a0);
        klog_print("--");
        /*--*/
        SYSCALL(TERMINATE,0,0,0);
        return *a0;
    }

    
    /**Se length = 0 non devo fare check per endAddr ma solo startingAddr.
    * Per controllare indirizzi validi devo essere sicuro che starting e ending siano: >= 0x8000.0000 ed entrambi <= 0x8001.EFFF 
    * Oppure Stack:  0xC000.0000 > addr >= 0xBFFF.F000
    **/
    if(length == 0){
        /*debug*/
        klog_print("dim = 0");
        klog_print_dec(*a0);
        klog_print("--");
        /*--*/
        *a0 = 0;
        return *a0;
    } 
    //devo fare p sul semaforo associato al processo. poichè ogni processo è associato o a master o a shell
    SYSCALL(PASSEREN,(int)&writeTermsemaphore,0,0);
    //check logical addr
    unsigned int endAddr = startingAddr +(length-1);
    //estraggo i bit alti degli indirizzi
    unsigned int vpnStart = startingAddr >> 12;
    unsigned int vpnEnd   = endAddr  >> 12;
    //Caso normale o caso stack
    int retVal = 0;
    //indirizzo terminale IntLine = 7: ((unsigned int) START_DEVREG +((7-3)0x80)+(devNumber0x10)) formula della phase2;
    /** termreg_t *term = (termreg_t *)DEV_REG_ADDR(IL_TERMINAL, 0); macro semplificativa */
    termreg_t *term = (termreg_t*) DEV_REG_ADDR(IL_TERMINAL,0);
    if((vpnStart >= 0x80000 && vpnStart <=0x8001E)&&(vpnEnd >=0x80000 && vpnEnd <=0x8001E)){
        //do the writing
        for(int i = 0; i< length;i++){
            char* c =(char*) (startingAddr +i);
            unsigned int command =( *c << 8) | TRANSMITCHAR;
            retVal = SYSCALL(DOIO,(int)&term->transm_command,command,0);
            if((retVal & 0xff) != 5){ // DOIO ritorna: For character transmission and receipt, the status word, in addition to containing a device completion code, will also contain the character transmitted or received.
                *a0 = -retVal;
                /*debug*/
                klog_print("errore DOIO");
                klog_print_dec(*a0);
                klog_print("--");
                /*--*/
                SYSCALL(VERHOGEN,(int)&writeTermsemaphore,0,0);
                return *a0;
            }
            (*a0)++;
        }
    }else if((vpnStart >= 0xBFFFF && vpnStart <=0xBFFFF)&&(vpnEnd >=0xBFFFF && vpnEnd <= 0xBFFFF)){
        //do the writing
        for(int i = 0; i< length;i++){
            char* c =(char*) (startingAddr +i);
            unsigned int command =( *c << 8) | TRANSMITCHAR;
            retVal = SYSCALL(DOIO,(int)&term->transm_command,command,0);
            if(retVal== -1){
                
                SYSCALL(VERHOGEN,(int)&writeTermsemaphore,0,0);
                *a0 = -1;
                /*debug*/
                klog_print("Errore DOIO 2");
                klog_print_dec(*a0);
                klog_print("--");
                /*--*/
                return *a0;
            }
            (*a0)++;
        }
    }else{
        *a0=-1;
        SYSCALL(VERHOGEN,(int)&writeTermsemaphore,0,0);
        /*debug*/
        klog_print("--Errore Pagina non valida--");
        klog_print_dec(*a0);
        klog_print("--");
        /*--*/
        return *a0;
    }
    SYSCALL(VERHOGEN,(int)&writeTermsemaphore,0,0);
    /*debug*/
    klog_print("Corretto:");
    klog_print_dec(*a0);
    klog_print("--");
    /*--*/
    return *a0;
}

/**ReadTerminal*/
void Syscall5(support_t* sup){
    state_t* status = &sup->sup_exceptState[GENERALEXCEPT];
    unsigned int* a0 = &status->reg_a0;
    unsigned int startingAddr = status->reg_a1; //indirizzo dove storare dati del terminale
   
    termreg_t* term = (termreg_t*) DEV_REG_ADDR(IL_TERMINAL,0);
    char*c = (char*)startingAddr;
    *a0=0;
    if(checkAddress(startingAddr) == 0){ //indirizzo valido: possiamo leggere
        SYSCALL(PASSEREN,(int)&readTermsemaphore,0,0);
        while(1){
            if(checkAddress((unsigned int)c) != 0){
                SYSCALL(VERHOGEN,(int)&readTermsemaphore,0,0);
                Syscall2(sup);
            }
            int retVal = SYSCALL(DOIO,(int)&term->recv_command,(int)RECEIVECHAR,0);
            int statusCode = retVal & 0xff;
            int charRecv = (retVal >> 8) & 0xff; // char
            /*debug*/
            klog_print("--Valori: retVal,statusCode,CharRecv-");
            klog_print_hex(retVal);
            klog_print("--");
            klog_print_hex(statusCode);
            klog_print("--");
            klog_print_hex(charRecv);
            klog_print("--end valori--");
            /*--*/
            if(statusCode != 5){
                *a0= -statusCode;
                SYSCALL(VERHOGEN,(int)&readTermsemaphore,0,0);
                return;
            }
            if(charRecv == '\n'|| charRecv == '\r'){
                *c=(char)charRecv;
                (*a0)++;
                SYSCALL(VERHOGEN,(int)&readTermsemaphore,0,0);
                return;
            }
            *c=(char)charRecv;
            c++; //incremento l'indirizzo 
            (*a0)++;
        }
    }else{
        Syscall2(sup);
    }      

}
/** Execute registro a0 dice il numero e a1 dice l'asid */
void Syscall6(support_t* sup){
    state_t* stato = &sup->sup_exceptState[GENERALEXCEPT];
    //possibili check di condizione per il valore passato in a1
    processCreation(stato->reg_a1); // in registro a1 viene passato l'asid
    SYSCALL(PASSEREN,(int)&shellSemaphore,0,0);
    //il processo una volta terminato chiamera sys2 che farà la verhogen sullo shellsem.
    
}
int checkAddress(unsigned int address){
    address= address >> 12;
    if((address >= 0x80000 && address <=0x8001E)||(address ==0xBFFFF)){
        return 0;
    }else{
        return 1;
    }
}


