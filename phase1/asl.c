#include "./headers/asl.h"
/**
 * semd_t => int* è l'indirizzo che contiene il valore del semaforo. Viene passato da kernel o qualcuno in alto
 *        => s_procq è la lista dei processi bloccati sul semaforo(lista interna)
 *        => s_link è la lista di semafori attivi -> Kernel li utilizza per gestione(lista esterna)
 * Avremo una lista semd_h di semafori attivi e riconoscibili con la propria key definita da semAdd.
 */
static semd_t semd_table[MAXPROC];  // array di semafori
static struct list_head semdFree_h; // lista di semafori non ancora utilizzati
static struct list_head semd_h;     // lista di semafori attivi


void mkEmptyProcQ(struct list_head* head);
void klog_print(char *str);

//qualcosa non funziona: facendo gdb sul main il programma fa system halted [0] quando fa initASL

//inizializza semdFree_h con MAXPROC. analoga a quella delle queue.
void initASL() {
    INIT_LIST_HEAD(&semdFree_h);
    INIT_LIST_HEAD(&semd_h);
    for(int i = 0; i < MAXPROC;i++)
    {
       list_add(&semd_table[i].s_link,&semdFree_h);
    }

}

/**aggiunge in coda al semaforo associato alla key semAdd.
 * Se non c'è corrispondenza nella lista semd_h si inizializza un nuovo semaforo con key semAdd.
 */
int insertBlocked(int* semAdd, pcb_t* p) {



        struct list_head *iter;
        list_for_each(iter,&semd_h){
            semd_t *sem = container_of(iter,semd_t,s_link);
            if(sem->s_key == semAdd )
            {
                list_add_tail(&p->p_list,&sem->s_procq);
                p->p_semAdd = semAdd;
                return 0;
            }
            /** non ha trovato la corrispondenza e i semd_h è ordinata in ordine crescente(seguendo le istruzioni di pandOS) */
            else if (sem->s_key > semAdd)
            {
                /*controllo se effettivamente abbiamo semafori liberi da allocare */
                if(!list_empty(&semdFree_h))
                {
                    //devo estrarre un nuovo semd da semdFree_h
                    semd_t* new_sem = container_of(semdFree_h.next,semd_t,s_link);
                    list_del(&new_sem->s_link);
                    //inizializzo il nuovo semaforo attivato
                    new_sem->s_key = semAdd;
                    mkEmptyProcQ(&new_sem->s_procq);
                    list_add_tail(&new_sem->s_link,iter); //aggiungo alla lista dei sem Attivi
                  
                    list_add_tail(&p->p_list,&new_sem->s_procq); //aggiungo in coda il pcb alla lista dei processi associati al semaforo
                    p->p_semAdd = semAdd; //forse ridondante ma per sicurezza

                    return 0;
                }
            }

        }


        /*non ha trovato nella lista dove inserire il valore:
         o lista è vuota o è ultimo elemento*/

        if(!list_empty(&semdFree_h))
             {
                 //devo estrarre un nuovo semd da semdFree_h
                 semd_t* new_sem = container_of(semdFree_h.next,semd_t,s_link);
                 list_del(&new_sem->s_link);
                 //inizializzo il nuovo semaforo attivato
                 new_sem->s_key = semAdd;
                 list_add_tail(&new_sem->s_link,&semd_h);
                 mkEmptyProcQ(&new_sem->s_procq);
                 list_add_tail(&p->p_list,&new_sem->s_procq);
                 p->p_semAdd = semAdd; //forse ridondante ma per sicurezza

                 return 0;
             }


    return 1;

}

/** ricerca corrispondenza key, se non c'è NULL
 * altrimenti rimuovi primo nodo di s_procq associato a semAdd  e ritorna il puntatore al processo.
 * Se la coda diventa vuota allora rimuovi il semaforo da s_link:
 *      list_del dalla lista semd_h e list_add da semdFree_h*/
pcb_t* removeBlocked(int* semAdd) {
  struct list_head *iter;
  list_for_each(iter, &semd_h){
  semd_t *sem = container_of(iter,semd_t,s_link);
    if(sem->s_key == semAdd){
      if(list_empty(&sem->s_procq)) return NULL;
        struct list_head* first_el= sem->s_procq.next;
        pcb_t* pcb = container_of(first_el, pcb_t, p_list );

        list_del(first_el);
        INIT_LIST_HEAD(&pcb->p_list);
        pcb->p_semAdd = NULL;

        if(list_empty(&sem->s_procq)){
          sem->s_key =  NULL;
          list_del(&sem->s_link);
          list_add(&sem->s_link,&semdFree_h);
          
        }
        return pcb;
    }
  }
  return NULL;
}


/**
 * Questa non mi è chiara: perchè ovrei voler rimuovere dalla coda di un semaforo un processo preciso?
 * Trovo processo sulla coda definita da p(contiene riferimento al suo semaforo) e lo rimuove.
 * Se non c'è ritorna NULL altrimenti p
 * 
 */
pcb_t* outBlocked(pcb_t* p) {
  if(p == NULL || p->p_semAdd == NULL) return NULL;

  struct list_head *iter;
  list_for_each(iter, &semd_h){
    semd_t *sem = container_of(iter,semd_t,s_link);

    if (sem->s_key == p->p_semAdd){
      struct list_head *iter2;

      list_for_each(iter2, &sem->s_procq){
        pcb_t *pcb = container_of(iter2, pcb_t, p_list);

        if(pcb == p){ // tolto il controllo p_pid perchè rischioso se pid gestiti male.
          list_del(iter2);
          INIT_LIST_HEAD(&p->p_list);
          p->p_semAdd = NULL;

          if (list_empty(&sem->s_procq)){
          // rimuovo il sem dalla lista degli attivi e lo aggiungo a quella dei free.
          list_del(&sem->s_link); 
          list_add(&sem->s_link,&semdFree_h);
          sem->s_key = NULL; // per evitare brutte sorprese mettiamo a null la key.
          INIT_LIST_HEAD(&sem->s_procq); // reset del s_procq per sicurezza.
        }

          return p;
        }
      }
      return NULL;

    }

  }
  return NULL;
}
/**
 * Ritorna un puntatore al pcb che e' alla testa del semaforo(s_procq) associato alla key semAdd. Se non trova ritorna null.
 */
pcb_t* headBlocked(int* semAdd) {
  struct list_head *iter;
  list_for_each(iter, &semd_h){
    semd_t *sem = container_of(iter,semd_t,s_link);

    if(sem->s_key == semAdd){
      if(list_empty(&sem->s_procq)){
        return NULL;
      }
      return container_of(sem->s_procq.next, pcb_t, p_list );
    }
  }
  return NULL;
}
