#include "./headers/pcb.h"
#include <uriscv/const.h>


/** Si dichiara questo per poter utilizzare klog_print */
extern void klog_print(char *str);


static struct list_head pcbFree_h; //sentinella -> inizio lista
static pcb_t pcbFree_table[MAXPROC]; // array contenente i processi
static int next_pid = 1;

/**
 * Inizializza la lista per contenere tutti gli elementi presenti nell'array MAXPROC PCBs
 * Inizializzo la lista vuota. Poi inserisco tutti i pcbs dentro la lista.
 * Questa lista è la pool da cui possiamo attingere per creare nuovi processi
 */
void initPcbs() {
    INIT_LIST_HEAD(&pcbFree_h);
    for(int i = 0; i < MAXPROC;i++)
    {
       list_add(&pcbFree_table[i].p_list,&pcbFree_h);
       pcbFree_table[i].p_pid = next_pid;
       next_pid++;
    }

}
 /** Funzione breakpoint */
void bp(){
}

/**Questa funzione serve per aggiungere alla lista dei pcbs liberi il pcb puntato */
void freePcb(pcb_t* p) {
    list_add(&p->p_list,&pcbFree_h);
}
/**Questa funzione viene usata quando utilizziamo un pcb libero: lo inizializzamo a 0
 * per cancellare ogni traccia del suo utilizzo precedente e lo restituiamo pulito.
 */
pcb_t* allocPcb() {
    if(list_empty(&pcbFree_h))
        return NULL;
    pcb_t *elem = container_of(pcbFree_h.next,pcb_t,p_list);   //prendo il pbc_t che dovrò eliminare dalla lista
    list_del(&elem->p_list);

    //-- Inizializzo l'elemento rimosso --
    INIT_LIST_HEAD(&elem->p_list);
    elem->p_parent        = NULL;
    INIT_LIST_HEAD(&elem->p_child);
    INIT_LIST_HEAD(&elem->p_sib);
    elem->p_s.entry_hi = 0;
    elem->p_s.cause    = 0;
    elem->p_s.status   = 0;
    elem->p_s.pc_epc   = 0;
    elem->p_s.mie      = 0;
    for (int i = 0; i < STATE_GPR_LEN; ++i) {
        elem->p_s.gpr[i] = 0;
    }
    elem->p_time          = 0;
    elem->p_semAdd        = NULL;
    elem->p_supportStruct = NULL;
    elem->p_prio          = 0;
    elem->p_pid           = next_pid++;


    return elem;
}
/** Elenco funzioni utilizzate:
 * INIT_LIST_HEAD(&testa_lista) => inizializza un valore già dichiarato di testa lista
 * list_add(&pcb->p_list,&testa_lista)=> aggiunge in testa il pcb passato
 * list_del(&pcb->p_list) => elimina il pcb puntato, gli passi il campo p_list per completare l'operazione
 * container_of(posizione,tipo,nome_campo_p_lista) => restituisce un puntatore alla struttura che contiene il p_list
 */

 /**inizializza la coda */
void mkEmptyProcQ(struct list_head* head) {
    INIT_LIST_HEAD(head);
}
/**Controlla se la coda è vuota */
int emptyProcQ(struct list_head* head) {
    return list_empty(head);
}
/** aggiunge un nodo alla coda nella posizione corretta: prio da max a min
 *  Costo computazionale O(n).
 *  Per migliorare -> struttura ausiliaria di lista puntatori all'ultimo elemento
 *  di un determinato valore di prio. Ma forse overkill.
 */
void insertProcQ(struct list_head* head, pcb_t* p) {
    if(emptyProcQ(head))
        list_add(&p->p_list,head);
    else
    {
        struct list_head *iter;
        list_for_each(iter,head){
            pcb_t *qpcb= container_of(iter,pcb_t,p_list);
            if(qpcb->p_prio < p->p_prio)
            {
                __list_add(&p->p_list,iter->prev,iter);
                return;
            }
            else if(list_is_last(iter,head))
            {
                list_add_tail(&p->p_list,head);
                return;
            }
        }
    }
}
/**ritorna un puntatore alla testa della coda
 * Head è sempre il valore sentinella.
*/
pcb_t* headProcQ(struct list_head* head) {

    if(list_empty(head))
        return NULL;

    struct list_head* firstNode = list_next(head);

    return container_of(firstNode,pcb_t,p_list);


}
/** rimuove il primo pcb dalla coda, se coda vuota ritorna NULL */
pcb_t* removeProcQ(struct list_head* head) {
    if(list_empty(head))
           return NULL;
    struct list_head* firstNode = list_next(head);
    pcb_t * nodo =container_of(firstNode,pcb_t,p_list);
    __list_del(head,nodo->p_list.next);
    return nodo;


}
/** rimuove il pcb puntato da p. Se esiste lo rimuove e ritorna il pcb altrimenti ritorna NULL */
pcb_t* outProcQ(struct list_head* head, pcb_t* p) {

    if(list_empty(head)) // controlliamo subito che la lista non sia vuota(superfluo)
        return NULL;
    struct list_head* iter = list_next(head); //inizializziamo iter alla testa della lista
    list_for_each(iter,head){
        pcb_t* qpcb = container_of(iter,pcb_t, p_list);
        if(qpcb->p_pid == p->p_pid ){
            list_del(iter);
            return p;
        }

    }
    return NULL;
}


/** Sezione Alberi */

/** emptychild ritorna TRUE se il pcb puntato non ha figli */
int emptyChild(pcb_t* p) {
  return list_empty(&p->p_child);
}
/**Rende il pcb p figlio del pcb prnt */
void insertChild(pcb_t* prnt, pcb_t* p) {
  // aggiungo alla lista dei figli di prnt il p_sib (che e' p)
  list_add(&p->p_sib, &prnt->p_child);
  p->p_parent = prnt;

}


/** rimuove il primo figlio del pcb p. Ritorna null se non ci sono figli, pcb altrimenti */
pcb_t* removeChild(pcb_t* p) {
  if (list_empty(&p->p_child)){
    return NULL;
  }
  pcb_t *child = container_of(p->p_child.next,pcb_t,p_sib);
  list_del(p->p_child.next); // p_child e' la sentinella (tipo head)
  child->p_parent = NULL;
  INIT_LIST_HEAD(&child->p_sib); // devo aggiornare i valori dei sibiling per evitare brutte sorprese
  return child;
}


/** Rimuove il link tra pcb  e il suo genitore. Se p non ha parenti ritorna null altrimenti p */
pcb_t* outChild(pcb_t* p) {
  if (p->p_parent == NULL) {
        return NULL;    // non è figlio di nessuno
    }
    // Rimuovi p dalla lista dei figli del padre
    list_del(&p->p_sib);
    // Segnala che non ha più un padre
    p->p_parent = NULL;

    return p;
}
