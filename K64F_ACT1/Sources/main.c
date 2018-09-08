#include "derivative.h" /* include peripheral declarations */
#include "os.h"
#include "datatypes.h"

char a,b,c,x;

void taskA(){
	a = 5;
	b = 10;
	activateTask(1,FALSE);		//User-defined index for task B
	x=0;
	/*if(readMailbox(0,&x)){
		wait();
		readMailbox(0,&x);
	}*/
	c = c + a + b + x;
	terminateTask();
}

void taskB(){
	c++;
	//if(writeMailbox(0,5)) wait();	//writeMailbox returns 1 if write was unsuccesfull
	//terminateTask();
	chainTask(2);
}

void taskC(){
	c+=2;
	//activateTask(1,FALSE);
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
	
	NVICICER1|=1<<PORTA_MASK;	//Clear PORT A interrupt flag
	NVICISER1|=1<<PORTA_MASK; 	//Set IRQ Vector for PORT A (Vector 59)
	
	createTask(0,0,TRUE,&taskA);
	createTask(1,1,FALSE,&taskB);
	createTask(2,2,FALSE,&taskC);
	
	/*createAlarm(0,0,20,TRUE,FALSE);		//Alarm 0 will activate task 0 every 20ms recurrently
	createAlarm(1,1,1000,FALSE,TRUE);	//Alarm 1 will activate task 1 only once after 1000ms
	createAlarm(2,2,200,TRUE,FALSE);	//Alarm 2 created but not enabled
	createAlarm(3,2,55,TRUE,FALSE);		//Alarm 3 will activate task 2 every 55ms recurrently
	createAlarm(4,0,30,TRUE,FALSE);		//Alarm 4 will activate task 0 every 30ms recurrently
	createAlarm(5,2,150,FALSE,FALSE);	//Alarm 5 will activate task 2 only once after 150ms
	createAlarm(6,1,40,TRUE,FALSE);		//Alarm 6 will activate task 1 every 40ms recurrently
	createAlarm(7,0,20,TRUE,FALSE);		//Alarm 7 created but not enabled
	createAlarm(8,1,20,TRUE,FALSE);		//Alarm 8 created but not enabled
	createAlarm(9,0,358,FALSE,FALSE);	//Alarm 9 will activate task 0 only once after 358ms
	
	createMailbox(0,&taskB,&taskA);		//Mailbox 0 has taskB as producer and TaskA as consumer*/
	
	OS_init();
	
	while(1);
	return 0;
}
