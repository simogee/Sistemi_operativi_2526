#include <uriscv/liburiscv.h>

#include "../headers/const.h"
#include "h/print.h"
#include "h/tconst.h"



/**
 * Necessario un mapping tra asid e nome del programma.
 * Useremo Execute(SYS6) per lanciare i programmi: Execute prende come input l'asid.
 * Per scegliere quale asid assegnare al programma corretto bisogna guardare il file config_machine.json
 */

struct asidToProgram{
    char* programName;
    int asidNum;
};

static struct asidToProgram map[]={
    {"fibEight",2},
    {"echo",3},
    {"fibEleven",4},
    {"uname",5},
    {"date",6},
    {"sl",7},
    {"calc",8}
};
void removeNewLine(char* str,int len);
int strncmpEnhanced(const char* firstArg, const char* secondArg, int szFirst,int szSecond);
int stringSize(const char* str);

void main(){
    while(1){
        char buff[64];
        int len =SYSCALL(READTERMINAL,(int)buff,0,0);
        removeNewLine(buff,len); //inutile, basta rimuovere l'ultimo char
        
        //fino a qui ok
        for(int i = 0; i < 7; i++){
            int nameLen = stringSize(map[i].programName); 
            if(strncmpEnhanced(buff,map[i].programName,len-1,nameLen)==0){
                print(WRITETERMINAL,"\n");
                SYSCALL(EXECUTE,map[i].asidNum,0,0);
                break;
            }else{
                if(i == 6){
                    print(WRITETERMINAL,"Invalid command\n");
                    break;
                }   
            }
        }
        
    }
        SYSCALL(TERMINATE,0,0,0);
}
    
//Bisogna comparare prima la lunghezza delle due stringhe e poi i char. copiata e riadattata da wikibooks
//Prima di usare strncmp per confrontare il risultato di READTERM con i programmi possibili è necessario modificare il char \n di READTERM in \0
int strncmpEnhanced(const char* firstArg, const char* secondArg, int szFirst,int szSecond)
{
    //se viene passata una stringa nulla
    if(szFirst == 0 || szSecond == 0){
        return -5;
    }
    if(szFirst != szSecond){
        return -5;
    }
    const char* ptr1 =  firstArg;
    const char* ptr2 =  secondArg;
    for(int i = 0; i < szFirst;i++){
        if(ptr1[i] < ptr2[i]){
            return -1;
        }else if(ptr1[i] > ptr2[i]){
            return 1;
        }
    }
    return 0;


}
//rimuove il \n char dal risultato di readTerm: aggiungo len come controllo di sicurezza
void removeNewLine(char* str,int len){
    char * ptr = str;
    int i = 0;
    while((i < len)&& (*ptr != '\n' )){
        ptr++;
        i++;
    }
    if(*ptr != '\n'){
        return;
    }else{
        *ptr = '\0';
    }
    
}
//ritorna lunghezza stringa;
int stringSize(const char* str){
    const char* ptr = str;
    int len = 0;
    int sizecheck = 0;
    while(sizecheck < 128 &&*ptr != '\0' && *ptr != '\n'){
        len++;
        ptr++;
        sizecheck++;
    }
    if(sizecheck == 127){
        return -1;
    }else{
        return len;
    }

}