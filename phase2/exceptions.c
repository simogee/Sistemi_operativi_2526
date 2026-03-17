#include <uriscv/const.h>
#include <uriscv/types.h>
#include "kernel.h"
/** tlb refill da fare o no? presente gia' in p2test */
void exception_handler();
void syscallHandler();
void uTLB_RefillHandler();
int* sem_index_from_dev(int IntlineNo, int devNo,memaddr inneroffset);

/*void uTLB_RefillHandler() {
setENTRYHI(0x80000000);
setENTRYLO(0x00000000);
TLBWR();
LDST((state_t*) BIOSDATAPAGE);
}*/

void subTree_killer(pcb_t* p);

void exception_handler(){

    state_t* ptr_exc = (state_t*) BIOSDATAPAGE; // puntiamo al bios_datapage per poter estrarre i valori dei campi necessari alla corretta gestione dell'exception
    unsigned int cause = ptr_exc->cause;//getCAUSE();

    if(CAUSE_IS_INT(cause))
    {
        //interruptHandler(ptr_exc); //questo sara' in un altro file.
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
    ptr_exc->pc_epc += WORDLEN;
    LDST(ptr_exc);
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
  process_counter++;
  ptr_exc->pc_epc += WORDLEN;
  LDST(ptr_exc);
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
    // linea 3 disk, 4 flash, 5 ethernet,6 printer. ognuno ha 8 device 32 totali.
    // linea 7 terminali con 2 sub-device 16 totali
    // 1 e' pseudoclock
        /* Bisogna quindi scegliere una convenzione: [0..7] disk [8..15] flash [16..23] eth [24..31] printer. [32..47] terminali [48]-> pseudo_clock_sem*/
        /* un device e' individuato da un interrupt line(valore 3,4,5) e un device number. Per i terminali anche un valore tx rx per indicare che tipo e'
            Ho indirizzo di COMMAND del device. da questo devo dedurre la linea e dev num
            devAddrBase = 0x10000054 + ((IntlineNo - 3) * 0x80) + (DevNo * 0x10)
            to calculate the device number you can use a series of ifs with bitwise AND (&) between the bitmap and the DEVxON constants as conditions.
            base address per i device: 0x10000054   
            per i device normali : (base) + 0x4 command
            per i terminali: 
            tx: (base) + 0xc
            rx: (base) + 0x4
            quindi se sottraggo 0x4 per un device normale ottengo l'indirizzo base del device. con l'indirizzo base devo trovare un modo per indicare la linea: ogni device occupa 0x10 indirizzi quindi
            0x80 è una linea. ora devo contare a quanti indirizzi disto dalla base 0x10000054 e ricavo così la linea di appartenenza poi per capire il deviceno: resto della divisione e conto l'offset con il resto.
            poi all'indice calcolato della linea devo aggiungere 3 per convenzione.   offset = devBase - 0x10000054; IntLineNo = 3 + (offset / 0x80); DevNo= (offset % 0x80) / 0x10;
            Numero della linea mi dice a quale blocco di semafori fare riferimento e il numero del device a quale di quelli della linea fare riferimento.

        */
        
        memaddr commandreg = ptr_exc->reg_a1;
        
        memaddr offset = (commandreg - 0x10000054);  // ritorna la distanza dall'indirizzo base dei devices.
        //con l'offset ora dobbiamo capire su quale linea e quale device ci si trova.
        memaddr inneroffset = offset % 0x10;  //quanto sono distante dall'inzio del device.
        if(inneroffset != 0x4 && inneroffset != 0xC){
            //errore
            PANIC();
        }
        memaddr devbase = commandreg - inneroffset; //abbiamo l'indirizzo base del device.
        memaddr devoffset = (devbase - 0x10000054); // troviamo l'offset del device 

        int IntlineNo = 3 + (devoffset / 0x80); // trovata la linea ora 
        if(IntlineNo < 3 || IntlineNo > 7){
            PANIC();
        }
        int devNo = (devoffset % 0x80) / 0x10; // trovato il device number.
        // ora bisogna mappare correttamente il semaforo alla linea e poi al device corretto
        /*linea 3:[0..7]disk linea 4:flash [8..15] linea 5:eth [16..23] linea 6:printer [24..31] linea 7:terminali [32..47] semaforo[48] e' lo pseudoclock*/
        int* semadr = sem_index_from_dev(IntlineNo,devNo, inneroffset); // ritorna il semaforo su cui verrà fatta la P
        if(semadr == NULL){ 
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
        setTIMER(TIMESLICE);
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
            case GETTIME:
                GetCPUTime(ptr_exc);
            case CLOCKWAIT:
                WaitForClock(ptr_exc);
            case GETSUPPORTPTR:
                GetSupportData(ptr_exc);
            case GETPROCESSID:
                GetProcessID(ptr_exc);
            case YIELD:
                Yield(ptr_exc);
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
 *      check se e' il current_process:
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
        if(inneroffset == 0x4){ // si tratta di un rx
            return &device_sem[devNo+32];
        }
        else if(inneroffset == 0xC){ // si tratta di un tx
            return &device_sem[devNo+40]; 
        }
        else{// errore 
            return NULL;
        }
        default:
            return NULL; 
    }

}