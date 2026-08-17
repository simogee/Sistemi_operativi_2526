/*#include <uriscv/liburiscv.h>

#include "h/tconst.h"
#include "h/print.h"

void main() {
    char stringa[128];
    int len=SYSCALL(READTERMINAL,(int)stringa,0,0);
    SYSCALL(WRITETERMINAL,(int)stringa,len,0);
    SYSCALL(TERMINATE, 0, 0, 0);
}
*/

#include <uriscv/liburiscv.h>

#include "h/tconst.h"
#include "h/print.h"

//riscv non supporta float apparentemente, quindi svolgo tutto con int
char op[]={'+','-','*','/'};
int result; 
int reminder;
int lenRes=0;
char res[4]; //stringa del risultato

void numToString(int res,int reminder,char buf[]);

void main() {
    char buf[4]; // numero simbolo numero \n
    int len = SYSCALL(READTERMINAL,(int)buf,0,0);
    if(len > 4){
        print(WRITETERMINAL,"errore Lunghezza valori");
        SYSCALL(TERMINATE,0,0,0);
    }
    char operation = buf[1]; // dove si trova l'op
    int firstVal = buf[0];
    int secondVal = buf[2];
    int num1 = firstVal   - '0';
    int num2 = secondVal  - '0';
    int index = -1;
    //confrontiamo con le operazioni nel nostro array di stringhe e se la troviamo l'eseguiamo altrimenti terminate.
    for(int i = 0; i < 4 ; i++){
        if(op[i] == operation){
            index = i;
            break;
        }
    }
    // fin qui ok

   switch(index){
        case (0):
            result =  num1 + num2;
            break;
        case (1):
            result=   num1 - num2;
            break;
        case (2):
            result=   num1 * num2;
            break;
        case (3):
            if(num2 == 0){// divisione per 0
                print(WRITETERMINAL,"Divisione per 0");
                SYSCALL(TERMINATE,0,0,0); 
            }
            result =   num1 / num2;
            if(num2 > num1){
                reminder = num1*10;
                int iter = 0;
                while((num2*iter) < reminder){
                    iter++;
                }
                reminder = iter;
            }else{
                reminder = num1 -(result * num2);
                reminder = ((reminder*10) / num2)*10;
            }
            
            break;
        default:
            print(WRITETERMINAL,"Errore operazione non riconosciuta");
            SYSCALL(TERMINATE,0,0,0);
    }
   
    numToString(result,reminder,res);
    for(int i = 0; i< 5;i++){
        if(res[i] == '\0'){
            lenRes= i;
            break;
        }
    }
    //scrivo il risultato sul terminale
    SYSCALL(WRITETERMINAL,(int)res,lenRes,0);
   
    SYSCALL(TERMINATE,0,0,0);
 
}

/**devo convertire il risultato in char.
 * casi:  < 0 dobbiamo riservare un char al segno -
 *        0-9: un char ad intero ed eventuale calcolo di remider. quindi: intero +'.'+ reminder
 *        10-81: due char per cifre e basta.
 * */
void numToString(int res,int reminder,char buf[]){
    if(res < 0){// qui è possibile solo con sottrazione
        buf[0]= '-';
        res = res *(-1); //gli cambio il segno
        buf[1] = (char)(res+'0');
        buf[2] = '\0';
    }else if(res >=0 && res <=9){
        buf[0] = (char)(res+'0');
        if(reminder != 0){
            buf[1] = '.';
            buf[2] = (char)(reminder+'0');
            buf[3] = '\0';
        }else{
            buf[1] = '\0';
        }
        
    }else{
        buf[0] = (char)((res/10)+'0');
        buf[1] = (char)((res %10)+'0');
        buf[2] = '\0';
    }
  return;
}