#include "supportL.h"
/* dove inizializziamo i processi utente
Primo: inizializziamo processo test/process_initiatior che dovrà:
    inizializzare strutture dati condivise: swap pool table, swap pool sem, device sem.
    Lanciare 1-8 processi utente
    Attende il termine dei processi lanciati.
Dobbiamo inizializzare le strutture dati condivise tra 
*/

support_t supportTable[UPROCMAX]; // tabella statica dove per ogni processo con ASID viene salvata la struttura support_t. Ogni PCB ci può accedere tramite support_t *p_supportStruct;

int masterSemaphore;
int shellSemaphore;

int flashSemaphore[UPROCMAX];

int readTermsemaphore;
int writeTermsemaphore;
// dall'asid seleziono il supportTable[asid-1] e poi guardo i campi della support_t(headers/types.h) e li aggiorno


void initSupportStructure(int asid){
    support_t supportProc = supportTable[asid-1];
    supportProc.sup_asid = asid;
    //state_t pc_epc = indirizzo istruzione dove partire/riprendere. status: descrive lo stato generale della CPU: modalità(qui user), interrupt(si), modo di ritorno. MIE maschera interrupt abilitati. cause = causa eccezione entry_hi= VPN, asid
    //cercare i vari indirizzi standard tra le definizioni
    //pagefaultcontext-> bisogna indicare il modo e indicare chi si occupa di gestire il pagefault(indirizzo pager),stackptr e status.
    //generalexceptioncntext ugale a sopra
    //inizilizzare il page table


}


void initDevSemaphore(int fls_dev){
    for(int i = 0; i< UPROCMAX;i++){
        flashSemaphore[i] = 1;
    }
}
//prende da support table supportTable[asid-1],inizializza la support struct(initSupportStructure), prepara lo state iniziale-> registri puntati correttamente, user mode, interrupt abilitati, asid in entry_hi e chiama create process(Kernel)
void processCreation(int asid){

}
/**inizializza swap pool table e semaforo, inizializza tutti i semafori, crea processo shell, fa P su masterSemaphore e poi TermProcess(kernel) */
void test(){
    masterSemaphore = 0;
    shellSemaphore = 0;

    readTermsemaphore = 1;
    writeTermsemaphore = 1;

}