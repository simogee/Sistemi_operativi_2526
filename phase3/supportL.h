#ifndef SUPPORT_LV4_H
#define SUPPORT_LV4_H

#include "../headers/types.h"
#include "../headers/const.h"

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
 */
/*semafori per garantire che processo padre non lasci orfani i figli */
extern int masterSemaphore; //semaforo del primo processo: viene fatta V solo quando la shell lanciata termina init 0
extern int shellSemaphore; //semaforo di tutti i processi lanciati dalla shell. viene fatta la V solo quando l'ultimo processo lanciato muore. init 0

/*semaforo per garantire mutua esclusione dei flash device*/
extern int flashSemaphore[UPROCMAX]; // usati per mutua esclusione tutti init a 1

/*semafori per lettura/scrittura shell*/
extern int readTerm;
extern int writeTerm;



/*funzioni generali*/
//inizializzazione strutture condivise(swap pool table)
void initSwapPoolTable();
//creazione U-proc
void process_creation(){

}
//pager
//general exception handler
//program trap handler

#endif