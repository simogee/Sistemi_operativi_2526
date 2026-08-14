#include <uriscv/liburiscv.h>

#include "h/tconst.h"
#include "h/print.h"

void main() {
    char* stringa = "Miao";
    SYSCALL(WRITETERMINAL,(int)stringa,5,0);
    SYSCALL(TERMINATE, 0, 0, 0);
    
    while (1);
}
