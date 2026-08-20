
#include "kernel.h"

void exception_handler();
void syscallHandler();
void uTLB_RefillHandler();
int* sem_index_from_dev(int IntlineNo, int devNo,memaddr inneroffset);
void interruptHandler(state_t* ptr_exc);

void passup_or_die(int index);




void subTree_killer(pcb_t* p);


void exception_handler(){

    state_t* ptr_exc = (state_t*) BIOSDATAPAGE;// state_t *ptr_exc = GET_EXCEPTION_STATE_PTR(getPRID()); // puntiamo al bios_datapage per poter estrarre i valori dei campi necessari alla corretta gestione dell'exception
    unsigned int cause = ptr_exc->cause;

    if(CAUSE_IS_INT(cause)) //check most significant bit
    {
        interruptHandler(ptr_exc); //questo sara' in un altro file.
    }
    else{
        unsigned int cause_code = cause & CAUSE_EXCCODE_MASK; //valore del registro cause e con la maschera CAUSE_EXCCODE_MASK ritorniamo il codice dell'eccezione
        if (cause_code == 8 || cause_code == 11)
            syscallHandler(ptr_exc);
        else if (cause_code >= 24 && cause_code <= 28){
            klog_print("PAGEFAULT");
            passup_or_die(PGFAULTEXCEPT);
        } else if( (cause_code >=0 && cause_code <= 7) || (cause_code == 9 || cause_code == 10)||(cause_code>=12 && cause_code<=23)){
            klog_print("GENERALEXCPT");
            passup_or_die(GENERALEXCEPT); 
        }else
            PANIC();
    }

}


/** Per tutte le SYSTEMCALL vengono passati i parametri utilizzati nei registri a0-a3. a0 contiene il tipo di chiamata da effettuare*/
//il pid si incrementa con allocPcb() in automatico
void create_process(state_t* ptr_exc){
  pcb_t* new_proc = allocPcb(); // Nota: di defult:
                                // p_time = 0
                                // p_semAdd = NULL
                                // viene generato un valore di p_pid
  if (new_proc == NULL){ // non c'e spazio -> salvo -1 nel registro s0 del padre
    ptr_exc->reg_a0 = -1;
    ptr_exc->pc_epc += WORDLEN;
    LDST(ptr_exc);
  }
  else{
    ptr_exc->reg_a0 = new_proc->p_pid; // c'e' spazio -> salvo il pid del figlio nel reg a0 del padre
    new_proc->p_s = *((state_t*) ptr_exc->reg_a1); // a1 (del padre) ha lo status di p_s del figlio: il padre deve preparare uno state_t da passare al figlio.
    new_proc->p_prio =  ptr_exc->reg_a2;
 
    new_proc->p_supportStruct = (support_t*) ptr_exc->reg_a3;

  
    insertChild(current_process, new_proc);
    //nuovo processo va inserito nella testa della readyqueue
    insertProcQ(&ready_queue, new_proc);
    process_counter++;
    ptr_exc->pc_epc += WORDLEN;
    LDST(ptr_exc);
  }
 
}




/** SYSTEMCALL per terminare i processi */
/* se processo current eliminato chiama lo scheduler altrimenti incrementa pc e riprende*/
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

    outChild(process_to_kill); // stacchiamo il processo dalla radice
    subTree_killer(process_to_kill);
}
if(current_process != NULL){
    insertProcQ(&ready_queue,current_process);
    current_process = NULL;
}
scheduler();

}
void subTree_killer(pcb_t* p){

    while((emptyChild(p) == 0)){ // ricorsivo

        pcb_t* child = removeChild(p);
        subTree_killer(child);
    }
    // arriviamo all'ultimo processo dell'albero
    // si può trovare o su un semaforo o in readyqueue o è current process
    if(p == current_process){
        current_process = NULL;
    }
    else if(p->p_semAdd != NULL){ // è in un semaforo
        int *sem = p->p_semAdd;
        //distinguo semaforo device/timer da semafori non mantenuti da kernel
        if(p->p_semAdd >= &device_sem[0] && p->p_semAdd <= &device_sem[SEMDEVLEN-1]) // è un semaforo mantenuto dal kernel
        {
            outBlocked(p);
            soft_block_counter--;
        }
        else{ // semaforo non mantenuto dal kernel
            outBlocked(p);
            (*sem)++; // faccio una v per correggere lo stato
        }
    }
    else{ // readyqueue
        outProcQ(&ready_queue,p);
    }
    process_counter--;
    freePcb(p);
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
            klog_print("NO SEMAFORI LIBERI PASSAREN");
            bp();
            PANIC();  //non ci sono semafori liberi
        } 
        current_process = NULL; //dereferenzio il current process

        scheduler();
    }

}

void Verhogen(state_t* ptr_exc){
    //physical address sempre in a1
    //NON BLOCCANTE
    //se semaphore dopo incremento è 0 o positivo allora c'era un processo bloccato e lo sblocco.
    // poi riprende current 
    int* semaphore = (int*)ptr_exc->reg_a1;
    (*semaphore)++;
    if(*semaphore <= 0){ //c'era un processo in attesa
    pcb_t* process_to_awake = removeBlocked(semaphore);
        if(process_to_awake != NULL){ // lo rimetto in readyqueue
            insertProcQ(&ready_queue,process_to_awake);

        }
    }
    // non devo svegliare il processo
    ptr_exc->pc_epc+=WORDLEN;
    LDST(ptr_exc);
}

//passaren usata all'interno di DoIO. Sempre bloccante
void block_sync(int* semaddr, state_t* ptr_exc){

    (*semaddr)--;//decremento di 1 il valore del semaforo
    ptr_exc->pc_epc += WORDLEN;
    current_process->p_s = *ptr_exc; // salvo lo stato aggiornato sul pcb
    cpu_t now;
    STCK(now);
    current_process->p_time += now - slice_start; //salviamo il empo passato dal dispatch del processo
    //devo inserire il processo nel semaforo
    int check = insertBlocked(semaddr,current_process); //inserisce il processo nella coda del semaforo relativo.
    if(check == 1){
        klog_print("NOSEMAFORILIBERI BLOCK_SYNC");
        bp();
        PANIC();  //non ci sono semafori liberi
    }
    soft_block_counter++;
    current_process = NULL; //dereferenzio il current process
    scheduler();
}


//address of the command field in a1 -> P made on the semaphore that the Nucleus maintains for the I/O device indicated by the value in a1
//always block the Current Process on the ASL(sem qui sono sync)
//terminal device -> 2 sem (one input and one output)
//the value to be assigned in a2.
/**
Doio si occupa di scrivere il comando sul registro del device e poi bloccare il processo sul semaforo relativo al device. Una volta salvato tutto con pc aggiornato richiama lo scheduler. */
void DoIO(state_t* ptr_exc){
    //dovremo gestire i device normali e i terminali: nei device normali ritorna in reg_a0 la word dello status, nei terminali si ritorna status+ char inviato o ricevuto
    //linea 3 disk, 4 flash, 5 ethernet,6 printer. ognuno ha 8 device 32 totali.
    //linea 7 terminali con 2 sub-device 16 totali
    //1 e' pseudoclock
        /* Bisogna quindi scegliere una convenzione: [0..7] disk [8..15] flash [16..23] eth [24..31] printer. [32..47] terminali [48]-> pseudo_clock_sem*/
        /* un device e' individuato da un interrupt line(valore 3,4,5) e un device number. Per i terminali anche un valore tx rx per indicare che tipo e'
            Ho indirizzo di COMMAND del device. da questo devo dedurre la linea e dev num
            devAddrBase = 0x10000054 + ((IntlineNo - 3) * 0x80) + (DevNo * 0x10)
            to calculate the device number you can use a series of ifs with bitwise AND (&) between the bitmap and the DEVxON constants as conditions.
            base address per i device: 0x10000054   
            per i device normali : (base) + 0x04 command
            per i terminali: 
            tx: (base) + 0x0c
            rx: (base) + 0x04
            quindi se sottraggo 0x04 per un device normale ottengo l'indirizzo base del device. con l'indirizzo base devo trovare un modo per indicare la linea: ogni device occupa 0x10 indirizzi quindi
            0x80 è una linea. ora devo contare a quanti indirizzi disto dalla base 0x10000054 e ricavo così la linea di appartenenza poi per capire il deviceno: resto della divisione e conto l'offset con il resto.
            poi all'indice calcolato della linea devo aggiungere 3 per convenzione.   offset = devBase - 0x10000054; IntLineNo = 3 + (offset / 0x80); DevNo= (offset % 0x80) / 0x10;
            Numero della linea mi dice a quale blocco di semafori fare riferimento e il numero del device a quale di quelli della linea fare riferimento.

        */
        //DEVREGLEN = ampiezza di un registro all'interno del device
        memaddr commandreg = ptr_exc->reg_a1;
        
        memaddr offset = (commandreg - START_DEVREG);  // ritorna la distanza dall'indirizzo base dei devices(START_DEVREG).
        //con l'offset ora dobbiamo capire su quale linea e quale device ci si trova.
        memaddr inneroffset = offset % DEVREGSIZE;  //quanto sono distante dall'inzio del device.
        if(inneroffset != 0x04 && inneroffset != 0x0C){
            //errore
            klog_print("INNEROFFSET errato");
            bp();
            PANIC();
        }
        memaddr devbase = commandreg - inneroffset; //abbiamo l'indirizzo base del device.
        memaddr devoffset = (devbase - START_DEVREG); // troviamo l'offset del device 

        int IntlineNo = 3 + (devoffset / (DEVREGSIZE * DEVPERINT)); // trovata la linea  
        if(IntlineNo < 3 || IntlineNo > 7){
            klog_print("intlineno errato");
            bp();
            PANIC();
            }
        int devNo = (devoffset % (DEVREGSIZE * DEVPERINT)) / DEVREGSIZE; // trovato il device number.(DEVREGSIZE è size di un device e DEVPERINT = numero di device per interrupt line)
        // ora bisogna mappare correttamente il semaforo alla linea e poi al device corretto
        /*linea 3:[0..7]disk linea 4:flash [8..15] linea 5:eth [16..23] linea 6:printer [24..31] linea 7:terminali [32..47] semaforo[48] e' lo pseudoclock*/
        int* semadr = sem_index_from_dev(IntlineNo,devNo, inneroffset); // ritorna il semaforo su cui verrà fatta la P
        if(semadr == NULL){ 
            klog_print("semaddr errato");
            bp();
            PANIC();
        }
        *((unsigned int*)commandreg) = ptr_exc->reg_a2;
        block_sync(semadr,ptr_exc); 

}

//ritornare in a0 il valore nel pcb_t in p_time
// non conta il tempo accumulato serve solo per sapere quanto tempo e' passato da inizio del quanto. Storia diversa se il processo era bloccato.
void GetCPUTime(state_t* ptr_exc){
    cpu_t now;
    STCK(now);
    ptr_exc->reg_a0 =current_process->p_time + now - slice_start;
    ptr_exc->pc_epc+=WORDLEN;
    LDST(ptr_exc);
}
//questo fa una P sul semaforo di pseudoclock: posizone 48
void WaitForClock(state_t* ptr_exc){
    block_sync(pseudo_clock_sem, ptr_exc);
}
void GetSupportData(state_t* ptr_exc){
    ptr_exc->reg_a0 = (memaddr)current_process->p_supportStruct; // se e' nulla ritornera' NULL
    ptr_exc->pc_epc+=WORDLEN;
    LDST(ptr_exc);
}

//ritorna il pid del padre del current process. pid del processo corrente se non c'e' un padre. ritorno in a0
void GetProcessID(state_t* ptr_exc){
    pcb_t* parent= current_process->p_parent;
    ptr_exc->pc_epc+= WORDLEN;

    if(ptr_exc->reg_a1 == 0){
        ptr_exc->reg_a0 = current_process->p_pid;
    }
    else{
        if(parent == NULL)
            ptr_exc->reg_a0 = 0;
        else
            ptr_exc->reg_a0 = parent->p_pid;
    }
    LDST(ptr_exc);
}
    //obbliga il processo corrente ad abbandonare la cpu. Se ci sono altri processi in coda e l'ex current ha la max prio non viene comunque eseguito subito
    //se invece e' l'unico processo allora viene eseguito
    //controllo se la coda e' vuota. se non e' vuota prendo il pcb del primo nella coda rimetto in coda il mio processo corrente e carico quello preso.(schedulo manualmente)
    //se la coda è vuota scheduler automatico
void Yield(state_t* ptr_exc){
    ptr_exc->pc_epc+=WORDLEN;
    cpu_t now;
    STCK(now);
    current_process->p_time += now- slice_start; // salvo il tempo
    current_process->p_s = *ptr_exc; // salvo lo stato aggiornato
    pcb_t* y_proc = current_process;
    current_process = NULL; //libero lo spazio del current proces
    if(emptyProcQ(&ready_queue)){//check se la coda e' vuota
        insertProcQ(&ready_queue,y_proc);
        scheduler();
   }else{
        pcb_t* substitute_proc = removeProcQ(&ready_queue);
        insertProcQ(&ready_queue,y_proc);
        current_process = substitute_proc;
        //schedulo manualmente
        STCK(slice_start); 
        setTIMER(TIMESLICE * (*((unsigned int *)TIMESCALEADDR)));
        LDST(&current_process->p_s);
   }
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
                DoIO(ptr_exc);
                break;
            case GETTIME:
                GetCPUTime(ptr_exc);
                break;
            case CLOCKWAIT:
                WaitForClock(ptr_exc);
                break;
            case GETSUPPORTPTR:
                GetSupportData(ptr_exc);
                break;
            case GETPROCESSID:
                GetProcessID(ptr_exc);
                break;
            case YIELD:
                Yield(ptr_exc);
                break;
            default:
                passup_or_die(GENERALEXCEPT);
        }
     }
     else if(syscallnum< 0 && (ptr_exc->status & MSTATUS_MPP_MASK) == MSTATUS_MPP_U){ // syscall privilegiata fatta in usermode
        ptr_exc->cause = PRIVINSTR; // errore di permesso
        passup_or_die(GENERALEXCEPT);
     }
     else{  // positive number syscall
        passup_or_die(GENERALEXCEPT);
     }
    }





// ritorna l'indice del semaforo data la line e il numero.
/*linea 3:[0..7]disk linea 4:flash [8..15] linea 5:eth [16..23] linea 6:printer [24..31] linea 7:terminali [32..47] semaforo[48] e' lo pseudoclock*/
int* sem_index_from_dev(int IntlineNo, int devNo,memaddr inneroffset){
    //switch case per line: se 3,4,5,6 allora cerco solo la posizione dato devNo e lo associo ad un semaforo
    //se 7 allora devo capire se è un dev di ricezione o di invio.

    switch(IntlineNo){
        //casi device normali 
        case 3: 
        //offset 0
        return &device_sem[devNo];
        case 4:
        //offset 8
        return &device_sem[devNo+8];
        case 5:
        //offset 16
        return &device_sem[devNo+16];
        case 6:
        //offset 24
        return &device_sem[devNo+24];


        //qui bisogna distinguere in che casistica ci troviamo e servira' inneroffset
        case 7:
        //offset 32 o 40
        if(inneroffset == 0x04){ // si tratta di un rx
            return &device_sem[devNo+32];
        }
        else if(inneroffset == 0x0C){ // si tratta di un tx
            return &device_sem[devNo+40]; 
        }
        else{// errore 
            return NULL;
        }
        default:
            return NULL; 
    }

}

 //** Sezione passup or die*/

 void passup_or_die(int index){
    if (current_process->p_supportStruct == NULL){ //die
        outChild(current_process);
        subTree_killer(current_process);
        scheduler();
    }else{
        current_process->p_supportStruct->sup_exceptState[index] = *((state_t*) BIOSDATAPAGE);  // salvo per passare exception a support level
        context_t ctx_proc= current_process->p_supportStruct->sup_exceptContext[index];  // questo per usare LDCXT : carico il contesto in una struttura piu' ordinata
        LDCXT(ctx_proc.stackPtr,ctx_proc.status,ctx_proc.pc);
    }
    
 }
 

 /** utlbrefill deve aver accesso alle strutture dati globali di phase2, indirizzo stack: 0x2000.0000(kernel stack)
 * Cosa deve fare: 1. accedere alla savedException(perchè siamo in un momento eccezione), questo si fa come in fase 2 dove prendiamo la saved_except dal biosDatapage.
 *                 2. avendo accesso al current process, cerchiamo nella sua supp struct la pagina che stiamo cercando(contenuta nella page_table)
 *                 3. Scriviamo: setEntryHI, setEntryLO e TLBWR(): pteEntry_t della pagina che ho determinato questa contiene:  pte_entryHI e LO, devo solamente passarli a set.
 *                 4. Come quando si finiva in phase2 LDST(), non è necessario incrementare il pc perchè al tentativo successivo non si verificherà l'eccezione.
 */

void uTLB_RefillHandler(){
    state_t* savedState = (state_t *)BIOSDATAPAGE; //review: biosDatapage è una zona di memoria in cui viene salvato lo stato della cpu al momento di un eccezione.
    pteEntry_t* pageTable = current_process->p_supportStruct->sup_privatePgTbl;  //otteniamo la page table del processo corrente.
    unsigned int pageMissing = ((savedState->entry_hi & GETPAGENO) >> VPNSHIFT); //dal registro entryHi usiamo la maschera GETPAGENO per isolare il VPN dal asid e poi shiftiamo a destra(contrario di quando si salva sul registro) per ottenere la pagina
    
    if(pageMissing == 0x3FFFF){ // è forse poco elegante ma fa il suo lavoro
        pageMissing=31;
    }
    /*Debug*/
    klog_print("--valore bit valid:");
    klog_print_hex(pageTable[pageMissing].pte_entryLO & VALIDON);
    klog_print("--");
    /*---*/
    setENTRYHI(pageTable[pageMissing].pte_entryHI);
    setENTRYLO(pageTable[pageMissing].pte_entryLO);
    TLBWR(); // scrive sul TLB
    LDST(savedState);
}