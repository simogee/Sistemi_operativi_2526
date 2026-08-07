#ifndef SUPPORT_LV4_H
#define SUPPORT_LV4_H

#include "../headers/types.h"
#include "../headers/const.h"
#include <uriscv/types.h>
#include "../phase2/kernel.h"
/**
 * In questa fase dobbiamo gestire le eccezioni che passavamo al livello superiore nella scorsa fase, che sono:
 *  TLB e page fault -> tlb è nei file della fase 2 ma dobbiamo aggiornarlo
 *  Syscall utente -> Non c'è molto da dire
 *  Program trap -> same as ^
 * In questa fase abbiamo delle strutture che sono condivise:
 *  swap pool table -> tiene traccia dei frame occupati: frame -> asid, logical page number associata e puntatore a matching entry della page table. E' condivisa
 *  !!la page table è privata per ogni processo!! ed è contenuta nella support_t supportTable -> tabella[asid] accedi a support_t del asid e la page table si trova nel pteEntry_t sup_privatePgTbl.
 *  Ogni entry di page table viene fatta coincidere con una entry di TLB: VPN,ASID,V,D. Ogni table ha 0-31(inizia a 0x8000.0000) pagine riservate a .text e .data e l'ultima è riservata allo stack
 *      le pagine sono grandi 0x1000.
 *  TLB: dati sono divisi in entryHI e entryLO: entryHI contiene VPN e asid. entryLo contiene PNF(frame) V= bit valid e D= bit dirty. 
 *       Quindi quando viene generato un indirizzo virtuale dal processo con asid= 2: 0x80003542 diventa: 0x80003 pagina e offset: 0x542. nella tlb viene cercata nel TLB tramite la coppia: (PGN,asid)
 *            --> (0x80003, 2) e controllando l'entryLO  relativo all'entryHI nel TLB, abbiamo: PFN, V, D. Una volta trovato frame corrispondente faremo indirizzo_start frame + offset.
 *  Rtornando alla page table abbiamo quindi un entryHI e un entryLO:
 *      entryHI: registro contenente: VPN e ASID.(in const.h) registro di 32 bit(VPNSHIFT = 12 e ASIDSHIFT = 6 ) quindi bit 31-12 sono di VPN e 11-6 di ASID. per creare entryHI si fa: "vpn or asid".
 *      Esempio: indirizzo virtuale generato da processo asid 4: 0x8003425- > vpn = 0x8003425 << 12 = 0x8003000.Per tenere offset dobbiamo fare and con una maschera per conservare i bit rimasti:0xFFFF. poi abbiamo asid = 4 quindi: asid = 4 << 6 = in binario 4: 10 diventa 10000000 = 128.
 *               ora dobbiamo fare l'or: 0x8003000 or 0x128 = 0x8003128 -> (vpn,asid). Per estrarre: VPN: entryHI >> 12. e per asid: entryHI >> 6(elimina i primi 6 bit) e dobbiamo conservare gli ultimi i 6 bit e basta: and 0x3f. Per fare queste operazioni controllare const.h
 *  Semafori:
 *  Swap pool semaphore -> necessario per evitare confusione e rischio di race condition. eg. un processo rimuove una pagina che serve ad un altro e un'altro rimuove la pagina appena inserita.
 * 
 * Storing dev: dal momento che non c'è memoria secondariao si può usare uriscv-mkdev per preloaddare i flash device con i file .out
 *              Ogni dispositivo quindi rappresenta un processo: viene preconfigurata la memoria del device con l'immagine del processo e il device diventa la memoria secondaria di quel processo
 * TLB refill:
 *  uTLB_RefillHandler => si occupa del cache miss event, deve inserire la pagina mancante nel tlb e restartare l'istruzione da prima dell'eccezione generata.(si trova in phase2)
 *  TLB_refill utilizza il primo frame di RAM come stack page, ha accesso sia alle strutture globali di lvl 2 che a quelle di lvl 3(strutture di supporto).
 *      Schema miss:
 *              1.localizzare tramite p_supportStruct la page table:
 *                 1.1 determinare la page_num "dissezionando" la entryHI nella saved_exception che si trova all'inizio del BIOS datapage.
 *                 1.2 prelevare la page_num trovata del processo corrente
 *              2. Scrivere nel TLB con TLBW:
 *                  2.1 setENTRYHI -> come spiegato sopra
 *                  2.2 setENTRYLO ->  TODO ma in breve i dati sono già scritti e se V = 0 allora è page fault e bisogna fare tutta la roba del caricamento della pagina
 *                  2.3 TLBW       ->  scrive tutto sul tlb
 *              3. Ritornare allo stato precedente con LDST
 * Swap Pool:
 *  Zona RAM dove memorizziamo frame -> pagine.
 *  Si può usare RAM dello stack page utilizzata per test in fase 2(WHAT??) oppure come ultimo frame della memoria starting address is: 0x20020000 (i.e. 0x20000000 + (32 * PAGESIZE)).
 *  !!Assicurarsi che nella config della macchina sia allocata RAM sufficiente!!
 * Struttura Swap pool Table:
 *  ASID, VPN, ptr alla relativa pagina della page_Table del processo.
 * Possibile usare ASID -1 per indicare frame libero
 * Size della tabella = size swap pool:1 entry per frame.
 * Pager:
 * E' il TLB_exception_handler(la fase 2 passava sopra delle eccezioni da gestire: questa è una di quelle)
 *  CASI:
 *      page fault su load op.  Invalid excp. TLBL
 *      page fault su store op. Invalid excp. TLBS
 *      Tentativo di scrittura non permessa. Modification excp. Mod -> questa non deve avvenire perchè tutte pagine sono marcate come read/write. Se accade è da gestire come trap.
 * Schema per gestione Page fault:(!!exception handler di questa fase possono interagire con nucleus e le sue strutture tramite syscall di valore negativo)
 *      1. ottenere puntatore a current proc support struct.: NSYS8
 *      2. Determinare la causa del TLB exception -> si trova nella current process support struct sup_exceptstate[0].
 *      3. Se è un modification bisogna gestirlo come Trap.
 *      4. Ottenere il mutual exclusion dello swap pool(P sul semaforo della pool)
 *      5. Determinare la pagina mancante che si trova nel registro entryHI della saved exception state
 *      6. Prendi un frame libero o da liberare con algoritmo per sceglierlo.
 *      7. Esaminare il frame:
 *          7.5 Se occupato: (pagina k del processo x) allora bisogna (atomico)segnare nella page table del processo x la pagina k con V = 0. Updatare TLB se necessario (atomico). Infine bisogna updatare il backing store: si scrive sul dispositivo x il contenuto del frame i appena liberato. Ogni errore è trap.
 *      8. Leggere il contenuto del backing data del processo corrente corrispondente alla pagina p e scriverlo sul frame i. Ogni errore è trap.
 *      9. Update la Swap pool table: frame i a pagina p del processo corrente 
 *      (Atomico)
 *     10. Update della page table dell current process : aggiornare la pagina p con V= 1 e frame i.
 *     11. Update del TLB per la entry p.
 *      (Atomico)
 *     12. Rilascia il semaforo della swap pool
 *     13. Ritorna allo stato precedente con LDST
 * Aggiornamento TLB:
 *  Ogni aggiornamento di page entry fatta dal pager deve controllare il tlb se quella pagina è presente in tlb.
 *      Approccio ultimo TLBP -> check se la TLB ha la entry che è stata modificata, dovrebbe tornare l'indice  e va riscritta con TLBWI
 *      Approccio semplificato: TLBCLR -> cancella tutte le entry(? what??)
 * Ordine operazioni del Pager:
 *  Quando aggiorni backing store  prima va fatto  update del page Table e TLB prima di scrittura su bckstr.
 *  Quando leggi dal backing store, prima lettura di aggiornamento page table e TLB.
 * Risposta al thought: caso scrittura BS prima di invalidazione: significa che il processo A può ancora accedere alla pagina k modificandone il contenuto, quindi potrebbe portare ad uno stato incoerente dei dati da scrivere in BS.
 *                      caso aggiornamento TLB e page table prima di scrittura in frame: processo B potrebbe richiedere accesso a frame F che però contiene ancora i dati vecchi.
 * Per entrambe situazioni è necessario disabilitare gli interrupt per permettere azioni atomiche.
 * Risposta al thought 2: Se page table e tlb non sono aggiornati atomically succede che ci troviamo in uno stato di incoerenza nelle tabelle:
 *                        Pagina P dev'essere invalidata, lo scriviamo su page Table. Avviene interrupt e legge TLB con la vecchia entry -> accediamo alla vecchia pagina che dovrebbe essere invalidata.
 *                        Pagina P dev'essere invalidata, lo scriviamo su TLB. Avviene interrupt, legge TLB: invalida, va in page table: valida, ricopia in TLB e accede ugualmente alla pagina che doveva essere invalida
 *                        Entrambi i casi violiamo la garanzia.
 * Algoritmo rimpiazzamento: static FIFO.
 * Support Level General Exception: SYSCALL livello utente, Trap.
 *              SYSCALL: terminate(wrapper), write e read terminal -> sospensione processo fino a fine del input/output, execute-> crea un processo, processo che chiama questa syscall viene sospeso fino a termine del sub-proc(P shellsem)
 * Trap handler: si chiama la terminate(wrapper): attenzione a rilasciare i semafori se questi erano stati presi.
 *
 */                     

/*semafori per garantire che processo padre non lasci orfani i figli */
extern int masterSemaphore; //semaforo del primo processo: viene fatta V solo quando la shell lanciata termina init 0
extern int shellSemaphore; //semaforo di tutti i processi lanciati dalla shell. viene fatta la V solo quando l'ultimo processo lanciato muore. init 0
extern int swapPoolSemaphore;// semaforo per garantire accesso esclusivo all'area di swap pool
/*semaforo per garantire mutua esclusione dei flash device*/
extern int flashSemaphore[UPROCMAX]; // usati per mutua esclusione tutti init a 1

/*semafori per lettura/scrittura shell*/
extern int readTermsemaphore;
extern int writeTermsemaphore;




/*funzioni generali*/
//inizializzazione test
void test();
//inizializzazione strutture condivise(swap pool table)
void initSwapPoolTable();
//creazione U-proc
void processCreation(int asid);
//pager
void pager();
//general exception handler
void *generalExceptionHandler();



#endif