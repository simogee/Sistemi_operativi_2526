#include "kernel.h"


//lower number line higher prio if two interrupt per device should solve the one with highest prio
//1. PLT highest prio 2. Interval Timer
//per terminali la trasmissione ha prio piu' alta rispetto ai recv

void interruptHandler(state_t* ptr_exc){
    unsigned int cause = ptr_exc->cause;
    cause = cause & CAUSE_EXCCODE_MASK; // ritorna il valore effettivo del interrupt
    int intline;
    switch(cause){
        //Process local timer IL_CPUTIMER intline 1
        case IL_CPUTIMER:
        intline = 1;
        break;
        //Interval timer IL_TIMER intline 2
        case IL_TIMER:
        intline = 2;
        break;
        //Disk device IL_DISK intline 3
        case IL_DISK:
        intline = 3;
        break;
        //Flash device IL_FLASH intline 4
        case IL_FLASH:
        intline = 4;
        break;
        //Ethernet device IL_ETHERNET intline 5
        case IL_ETHERNET:
        intline = 5;
        break;
        //Printer device IL_PRINTER intline 6
        case IL_PRINTER:
        intline = 6;
        break;
        //Terminal device IL_TERMINAL intline 7
        case IL_TERMINAL:
        intline = 7;
        break;
        //caso in cui arriva un valore non riconosciuto
        default:
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
   if(intline = 1){
    
   }

    //caso INTERVAL TIMER
    //**
    // ACK interrupt load intervaltimer 100ms(PSECONDS),LDIT(PSECONDS)
    // Unblock all PCBs waiting a pseudo-clock tick e put in readyqueue.  pseudo_clock_sem= [48] Fai una funzione di sblocco e decremento di soft_block_counter
    // return control to current process if exists LDST(ptr_exc); 
    //  */
   else if(intline = 2){

   }

    //caso Device-generico 
    // Va individuato il device che ha il pending interrupt, calcolato il device address base
    // salvare lo status code
    // scrivere ACK nel registro command del device
    // Fare una V sul semaforo relativo al device
    // salvare lo status code nel nuovo pcb registro a0
    // inserire il pcb appena sbloccato nella readyqueue
    // fare LDST sullo stato dell'eccezione della cpu oppure chiamare scheduler
   else if(intline < 2 && intline <=7){

   }

   else{
    PANIC();
   }

    
    
}