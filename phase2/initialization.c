

/**
 * 
 * Variabili da dichiarare:
 * Process count -> uint
 * Soft-block count -> uint, numero di processi partiti ma non ancora terminati
 * Ready queue -> coda di pcb in ready state quindi direi una list_head* 
 * Current Process -> pcb_t* puntatore al processo attualmente in running state
 * Device semaphore -> il kernel tiene un intero per ogni device esterno più uno per lo pseudo-clock. (Dal momento che i terminali sono due device indipendenti,
 * il kernel tiene due semafori per ogni terminale.)
 * 
 * 
 *   
 * Bisogna inizializzare il Pass-Up Vector -> il passup vector è un vettore i cui campi puntano alle funzioni interrupt handlers (indirizzo: 0x0FFFF900)
 * 
 * 
 * Inizializzare le strutture dati della phase1, includere quindi i file della fase precedente
 * (Bisognerà linkare i file della phase1 ?)
 * Inizializzare le strutture dati e variabili phase2 initPcbs() e initASL()
 * 
 * Load the system-wide interval Timer con 100 ms -> no idea atm.
 * 
 * Istanziare un process test
 * 
 * 
 * Chiamare lo scheduler
 * 
 */