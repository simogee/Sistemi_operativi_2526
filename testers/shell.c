#include <uriscv/liburiscv.h>

#include "h/tconst.h"
#include "h/print.h"

void main() {
    char stringa[128];
    SYSCALL(READTERMINAL,(int)&stringa,0,0);
    int len=0;
    while((stringa[len] != '\n') && (stringa[len] != '\r')){
        len++;
    }
    SYSCALL(WRITETERMINAL,(int)stringa,len,0);
    SYSCALL(TERMINATE, 0, 0, 0);
    
   
}
