#include "derivative.h" /* include peripheral declarations */
#include "os.h"
#include "datatypes.h"

char a,b,c;

void taskA(){
	a = 5;
	b = 10;
	activateTask(1,FALSE);		//User-defined index for task B
	c = c + a + b;
	terminateTask();
}

void taskB(){
	c++;
	//chainTask(2);
	terminateTask();
}

void taskC(){
	c*=2;
	terminateTask();
}

void PORTA_IRQHandler(void){	//ISR pushes XPSR, PC, LR, R12, R3, R2, R1, R0 in said order
	saveContext();
	PORTA_PCR1|=1<<24;		//w1c ISF (Interrupt Status Flag) por PTA1
	activateTask(1,TRUE);
}

int main(void){
	
	SIM_SCGC5=1<<9;			//Enable clock for PORT A
	
	PORTA_PCR1=(1<<8)+(9<<16);	//PTA1 as GPIO and interrupt enable on rising-edge
	
	NVICICER1|=1<<(59%32);	//Clear PORT A interrupt flag
	NVICISER1|=1<<(59%32); 	//Set IRQ Vector for PORT A (Vector 59)
		
	//Task A
	Tasks[0].PRIORITY=0;
	Tasks[0].AUTOSTART=TRUE;
	Tasks[0].TASK_INITIAL_ADDR=&taskA;
	//Task B
	Tasks[1].PRIORITY=1;
	Tasks[1].AUTOSTART=FALSE;
	Tasks[1].TASK_INITIAL_ADDR=&taskB;
	//Task C
	Tasks[2].PRIORITY=2;
	Tasks[2].AUTOSTART=FALSE;
	Tasks[2].TASK_INITIAL_ADDR=&taskC;
	
	OS_init();
	
	while(1);
	return 0;
}
