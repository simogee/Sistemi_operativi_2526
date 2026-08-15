#include <uriscv/liburiscv.h>

#include "h/tconst.h"
#include "h/print.h"

void main() {
    char stringa[128];
    int len=SYSCALL(READTERMINAL,(int)stringa,0,0);
    SYSCALL(WRITETERMINAL,(int)stringa,len,0);
    SYSCALL(TERMINATE, 0, 0, 0);
}
