#include <uriscv/liburiscv.h>

#include "h/tconst.h"
#include "h/print.h"

void main() {
    SYSCALL(TERMINATE, 0, 0, 0);

    while (1);
}
