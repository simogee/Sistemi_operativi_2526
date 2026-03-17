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
        default:
        intline = -1;
        break;
    }
    //ora abbiamo la linea in cui e' avvenuto un interrupt.
}