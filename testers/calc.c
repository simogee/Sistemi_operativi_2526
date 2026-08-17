#include <uriscv/liburiscv.h>

#include "h/tconst.h"
#include "h/print.h"

char*op[] ={"+","-","*","/"};
union{ int i; float f} result;
void main() {
    char buf[4]; // numero simbolo numero \n
    int len = SYSCALL(READTERMINAL,(int)buf,0,0);
    if(len > 4){
        SYSCALL(TERMINATE,0,0,0);
    }
    char operation = buf[1]; // dove si trova l'op
    int firstVal = buf[0];
    int secondVal= buf[2];
    int index = -1;
    //confrontiamo con le operazioni nel nostro array di stringhe e se la troviamo l'eseguiamo altrimenti terminate.
    for(int i = 0; i < 4 ; i++){
        if(*op[i] == operation){
            index = i;
            break;
        }
    }
   switch(index){
        case (1):
            result.i = firstVal + secondVal;
            break;
        case (2):
            result.i= firstVal - secondVal;
            break;
        case (3):
            result.i= firstVal * secondVal;
            break;
        case (4):
            result.f= firstVal / secondVal;
            break;
        default: 
            SYSCALL(TERMINATE,0,0,0);
   }
   if(index == 4){
    SYSCALL(WRITETERMINAL,(int)result.f,sizeof(float),0);
   }
   SYSCALL(WRITETERMINAL,(int)result.i,sizeof(int),0);
      //scrivo il risultato sul terminale
   SYSCALL(TERMINATE,0,0,0);
 
}
