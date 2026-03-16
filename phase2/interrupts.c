#include "kernel.h"


//lower number line higher prio if two interrupt per device should solve the one with highest prio
//1. PLT highest prio 2. Interval Timer
//per terminali la trasmissione ha prio piu' alta rispetto ai recv