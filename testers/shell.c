#include <uriscv/liburiscv.h>

#include "h/tconst.h"
#include "h/print.h"

void main() {
    char stringa[128];
    SYSCALL(READTERMINAL,(int)stringa,0,0);
    SYSCALL(WRITETERMINAL,(int)stringa,5,0);
    SYSCALL(TERMINATE, 0, 0, 0);
    
    while (1);
}
