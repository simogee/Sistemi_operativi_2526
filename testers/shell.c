#include <uriscv/liburiscv.h>

#include "h/tconst.h"
#include "h/print.h"

void main() {
    char buff[10];
    SYSCALL(READTERMINAL,(int)buff,0,0);
    SYSCALL(WRITETERMINAL,(int)buff,10,0);
    SYSCALL(TERMINATE, 0, 0, 0);
    
    while (1);
}
