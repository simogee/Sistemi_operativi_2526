#include "kernel.h"

//lower number line higher prio if two interrupt per device should solve the one with highest prio
//1. PLT highest prio 2. Interval Timer
//per terminali la trasmissione ha prio piu' alta rispetto ai recv
pcb_t* unblock_devicesem(int* semaddr);
void unblock_pseudoclock();

void interruptHandler(state_t* ptr_exc){
    
    unsigned int cause = ptr_exc->cause;
    unsigned int cause_code = cause & CAUSE_EXCCODE_MASK; 
  
    int intline;

    if (cause_code == IL_CPUTIMER) {
        intline = 1;
    }
    else if (cause_code == IL_TIMER) {
        intline = 2;
    }
    else if (cause_code == IL_DISK) {
        intline = 3;
    }
    else if (cause_code == IL_FLASH) {
        intline = 4;
    }
    else if (cause_code == IL_ETHERNET) {
        intline = 5;
    }
    else if (cause_code == IL_PRINTER) {
        intline = 6;
    }
    else if (cause_code == IL_TERMINAL) {
        intline = 7;
    }
    else {
        klog_print("PANICO interrupt handler");
        bp();
        PANIC();
    }
    //ora abbiamo la linea in cui e' avvenuto un interrupt.
    //bisogna trovare il device che lo ha invocato.
    //lower line and device indica priorita'

    //caso PLT
    /**ACK  per interrupt con loading timer usando setTIMER,
     * Copy the processor state of the current cpu at the time of the exception into current process -> p_s
     * place current process in readyqueue e rendi current process = NULL
     * chiama lo scheduler
    
    */
   if(intline == 1){
    cpu_t now;
    STCK(now);
    setTIMER(TIMESLICE); // serve come ack aggiorna il timer per evitare di rientrare subito sull'interrupt appena si riattivano gli interrupt
    //bisogna copiare lo stato del processore nello stato del current process
    current_process->p_s = *ptr_exc;
    current_process->p_time += now - slice_start;

    //pongo current process nella ready_queue;
    insertProcQ(&ready_queue,current_process);
    current_process = NULL;
    scheduler();
   }

    //caso INTERVAL TIMER
    //**
    // ACK interrupt load intervaltimer 100ms(PSECONDS),LDIT(PSECONDS)
    // Unblock all PCBs waiting a pseudo-clock tick e put in readyqueue.  pseudo_clock_sem= [48] Fai una funzione di sblocco e decremento di soft_block_counter
    // return control to current process if exists LDST(ptr_exc); altrimenti scheduler()
    //  */
   else if(intline == 2){
    //ack
    LDIT(PSECOND);
    //funzione per liberare la coda sull'indirizzo dello pseudo_clock_sem e mettere i processi in readyqueue. Qui  soft_block_counter va decrementato.
    unblock_pseudoclock();
    if(current_process != NULL){
        LDST(ptr_exc);
    }
    else{
        scheduler();
    }
   }

    //caso Device-generico 
    // Va individuato il device che ha il pending interrupt poi calcolato il device address base
    // salvare lo status code
    // scrivere ACK nel registro command del device
    // Fare una V sul semaforo relativo al device fatto.
    // salvare lo status code nel nuovo pcb registro a0
    // inserire il pcb appena sbloccato nella readyqueue
    // fare LDST sullo stato dell'eccezione della cpu oppure chiamare scheduler
   else if(intline > 2 && intline <8){ 
    //definisci per linea la bitmap su dove fare il & per trovare il device
    unsigned int bitmap = *((unsigned int*) CDEV_BITMAP_ADDR(cause_code)); //casting necessario perchè cdev_bitmap_addr ritorna l'indirizzo ci va cause_code e non intline
    // ora si fa un while e si trova il primo device della linea con un pending interrupt
    unsigned int devON = DEV0ON;
    int devNo = 0;
    while((bitmap & devON) == 0){
        devON <<= 1;
        devNo++;
        if(devNo > 7){
            klog_print("\ndev non trovato?"); 
            bp();
            PANIC();
        } 
    }
    //dovrebbe aver ritornato al primo device incontrato.
    memaddr devaddrb = DEVREGBASE + ((intline - 3) * (DEVPERINT * DEVREGSIZE)) + (devNo * DEVREGSIZE);
    //ora va salvato lo stato per dopo.
    //qui si diverge: una parte per i device normali e una per i device terminali


    if(intline< 7){ //device normale
       unsigned int status_save = *((unsigned int*)(devaddrb + STATUS * DEVREGLEN));//memaddr status_save = devaddrb + 0x0; 
       memaddr commandaddr = devaddrb + (COMMAND * DEVREGLEN);
       *((unsigned int*) commandaddr) = ACK;
       int* semaddr= sem_index_from_dev(intline,devNo,(COMMAND * DEVREGLEN));// linea, numero di device e offset del command register 0x4 in questo caso !!!!!! scope della funzione va reso visibile anche qui 
       pcb_t* unlocked_proc = unblock_devicesem(semaddr); // faccio la V e sblocco il processo
       if(unlocked_proc != NULL){
            unlocked_proc->p_s.reg_a0 = status_save;
            insertProcQ(&ready_queue, unlocked_proc);   
       }
       if(current_process != NULL)
            LDST(ptr_exc);
       else
            scheduler();

    }else{ // terminali
        //bisogna distinguere se è un recv terminal o trasmit terminal: 
        unsigned int recv_status = *((unsigned int*)(devaddrb + RECVSTATUS * DEVREGLEN));
        unsigned int tran_status = *((unsigned int*)(devaddrb + TRANSTATUS * DEVREGLEN)); // prendo entrambi gli status e poi confronto
        unsigned int status_save; // dove salverò poi lo status
        memaddr command_addr;
        int *semaddr;
        pcb_t *unlocked_proc;
        //trasmission is higher prio than recv
        if((tran_status & 0xFF) == OKCHARTRANS){
            status_save = tran_status;
            command_addr = devaddrb + TRANCOMMAND * DEVREGLEN;
            
            semaddr = sem_index_from_dev(7, devNo, TRANCOMMAND * DEVREGLEN);
        }
        else if((recv_status & 0xFF) != READY){
            status_save = recv_status;
            command_addr = devaddrb + RECVCOMMAND * DEVREGLEN;
            
            semaddr = sem_index_from_dev(7, devNo, RECVCOMMAND * DEVREGLEN);
        }else {
            klog_print("PANICO intline 7"); 
            bp();
            PANIC();
        }
            
        *((unsigned int*)command_addr) = ACK; // carico ACK
        unlocked_proc =unblock_devicesem(semaddr);
        if(unlocked_proc != NULL){
            unlocked_proc->p_s.reg_a0 = status_save;
            insertProcQ(&ready_queue, unlocked_proc);   
        }
        if(current_process != NULL){
            LDST(ptr_exc);
        }else
            scheduler();

        
    }
    }
   else{
        klog_print("PANICO intline non valida"); 
        bp();
    PANIC();
   }

    
    
}


//funzione che sblocca i processi fermi sul semaforo pseudoclock e riemtte in readyqueue i processi
//indirizzo pseudo_clock_sem
void unblock_pseudoclock(){
    int flag = 0;
    while(flag == 0){
        pcb_t* blocked_process = removeBlocked(pseudo_clock_sem);
        if(blocked_process == NULL){
            flag = 1;
        }else{
            insertProcQ(&ready_queue,blocked_process);
            soft_block_counter--;
        }
    }
    *pseudo_clock_sem =0; //reset del valore del semaforo

}
//same as before ma generica per i devices. si potrebbe collassare tutto in un unica funzione ma preferisco tenerle separate per una questione "didattica"
pcb_t* unblock_devicesem(int* semaddr){
    pcb_t* blocked_process = NULL;
    (*semaddr)++;
    if((*semaddr)<= 0)
    {
        blocked_process = removeBlocked(semaddr);
        if(blocked_process != NULL){
            soft_block_counter--;
        }
    }
    
    
    return blocked_process;

}