#include "derivative.h"
#include "datatypes.h"
#include "os.h"
#include <stdlib.h>

s16b runningTask=-1;		//Running task ID holder, if -1 there are no tasks running
u8b hpt,i;					//Highest Priority Task ID holder and control counter for tasks array sweeping
bool pendingTasks;			//Pending tasks flag
u32b *isr_sp;				//Pointer to SP when an ISR is issued
u32b main_pc,main_sp;		//Pointers to main function return address, used only when there are no more tasks ready

//System data structures - NOT FOR USER//////////
enum STATES {IDLE,READY,RUN,WAIT};
typedef struct{
	u8b PRIORITY;
	bool AUTOSTART;
	bool INTERRUPTED;
	/*
	 * Context indexes:
	 * 	0: R0
	 * 	1: R1
	 * 	2: R2
	 * 	3: R3
	 * 	4: R12
	 * 	5: LR
	 * 	6: PC - Return address
	 */
	u32b CONTEXT[CONTEXT_SIZE];
	u32b TASK_SP;
	void (*TASK_INITIAL_ADDR)();
	enum STATES STATE;
} Task;
Task Tasks[TASKS_LIMIT];

typedef struct{
	u32b CNT;
	u32b RELOAD;
	bool RECURRENT;
	bool ENABLE;
	u8b TASK_ID;
} Alarm;
Alarm Alarms[NUMBER_OF_ALARMS];

typedef struct{	//ID is array index
	u32b DATA;
	bool SEMAPHORE;
	void *PRODUCER;
	void *CONSUMER;
} Mailbox;
Mailbox *Mailboxes[NUMBER_OF_MAILBOXES];
/////////////////////////////////////////////////

//Interrupciones del SysTick ya estan deshabilitadas durante SystemCalls, asumimos que no hay mas interrupciones

//Local helper methods////////
void alarmSweep(void);
void restoreContext(void);
void schedule(void);
__attribute__( ( always_inline ) ) __STATIC_INLINE uint32_t __get_LR(void);
__attribute__( ( always_inline ) ) __STATIC_INLINE uint32_t __get_SP(void);
//////////////////////////////

//System calls for user//////////////////////////////////////////////////////////////////
__attribute__( ( always_inline )) __STATIC_INLINE uint32_t __get_SP(){
	register uint32_t result; 
	__ASM volatile ("MOV %0, SP\n" : "=r" (result) );
	return(result);
}

__attribute__( ( always_inline ) ) __STATIC_INLINE uint32_t __get_LR(){ 
	register uint32_t result; 
	__ASM volatile ("MOV %0, LR\n" : "=r" (result) ); 
	return(result); 
}

void schedule(void){
	runningTask=-1;
	u8b n=0;	//Mailboxes array counter
	hpt=0;		//Always assume the highest priority task is the first task in the array
	i=0;		//Reset counter
	do{
		if(Tasks[i].STATE==WAIT){		//If task was waiting, check semaphore
			do{
				if(Tasks[i].TASK_INITIAL_ADDR==Mailboxes[n]->PRODUCER){		//If waiting task is the mailbox´s producer
					if(Mailboxes[n]->SEMAPHORE==0) Tasks[i].STATE=READY;		//Check if semaphore is available
				}else if(Tasks[i].TASK_INITIAL_ADDR==Mailboxes[n]->CONSUMER){	//If waiting task is the mailbox´s consumer
					if(Mailboxes[n]->SEMAPHORE==1) Tasks[i].STATE=READY;		//Checkf if data is available
				}
			}while(n++<NUMBER_OF_MAILBOXES-1);
		}
		if((Tasks[i].PRIORITY >= Tasks[hpt].PRIORITY)&&(Tasks[i].STATE==READY)){
			hpt=i;
			runningTask=hpt;
		}
	}while(i++<TASKS_LIMIT-1);
	if(runningTask!=-1){
		Tasks[runningTask].STATE=RUN;
		runningTask=hpt;
	}
}

void createTask(u8b id, u8b priority, bool autostart, void *task_initial_addr){
	Tasks[id].PRIORITY=priority;
	Tasks[id].AUTOSTART=autostart;
	Tasks[id].TASK_INITIAL_ADDR = task_initial_addr;
}

void createAlarm(u8b alarm_index, u8b task_id, u32b reload, bool recurrent, bool enable){
	if(alarm_index>=NUMBER_OF_ALARMS) return;
	Alarms[alarm_index].TASK_ID=task_id;
	Alarms[alarm_index].RELOAD=reload;
	Alarms[alarm_index].CNT=reload;
	Alarms[alarm_index].RECURRENT=recurrent;
	Alarms[alarm_index].ENABLE=enable;
}
 
void createMailbox(u8b id, void *producer, void *consumer){
	Mailbox* mb = malloc(sizeof(Mailbox));
	mb->PRODUCER=producer;
	mb->CONSUMER=consumer;
	mb->SEMAPHORE=FALSE;
	mb->DATA=0;				//Init data in cero to prevent errors
	Mailboxes[id]=mb;
}

bool writeMailbox(u8b id, u32b data){
	if(Mailboxes[id]->PRODUCER!=Tasks[runningTask].TASK_INITIAL_ADDR) return 1;	//If the task trying to write to mailbox is not its producer, return 1
	SYST_CSR|=1<<0;			//Clear TICKINT bit fields in SysTick Control and Status Register to disable interrupt
	register bool *reg=&Mailboxes[id]->SEMAPHORE;
	bool busy,claimed;
	asm("MOV R0,%0\n"::"r" (reg):"r0");				//R0 holds semafore address
	asm("LDREXB R1, [R0]");
	asm("CPY %0, R1\n":"=r" (busy)::"r1");
	asm("MOV R1, #1");
	if(!busy){
		asm("STREXB R4,R1,[R0]");		//R4 for status of Exclusive Store Byte
		asm("MOV %0, R4":"=r" (claimed)::"r4");
		if(!claimed){						//0 indicates that system function claimed the semaphore address
			Mailboxes[id]->DATA=data;		//Write data to mailbox
			SYST_CSR|=1<<1;			//Set TICKINT bit fields in SysTick Control and Status Register to enable interrupt
			return claimed;
		}else{
			SYST_CSR|=1<<1;			//Set TICKINT bit fields in SysTick Control and Status Register to enable interrupt
			return claimed;
		}
	}else{
		SYST_CSR|=1<<1;			//Set TICKINT bit fields in SysTick Control and Status Register to enable interrupt
		return busy;
	}
}

u32b readMailbox(u8b id,u32b *addr){
	if(Mailboxes[id]->CONSUMER!=Tasks[runningTask].TASK_INITIAL_ADDR) return 1;	//If task trying to read mailbox is not its consumer, return 0
	SYST_CSR|=1<<0;			//Clear TICKINT bit fields in SysTick Control and Status Register to disable interrupt
	register bool *reg=&Mailboxes[id]->SEMAPHORE;
	bool available,claimed;
	asm("MOV R0,%0\n"::"r" (reg):"r0");				//R0 holds semafore address
	asm("LDREXB R1, [R0]");
	asm("CPY %0, R1\n":"=r" (available)::"r1");
	asm("MOV R1, #0");
	if(available){
		asm("STREXB R4,R1,[R0]");		//R4 for status of Exclusive Store Byte
		asm("MOV %0, R4":"=r" (claimed)::"r4");
		if(!claimed){					//0 indicates that system function claimed the semaphore address
			*addr=Mailboxes[id]->DATA;	//Write mailbox data to desired address
			SYST_CSR|=1<<1;				//Set TICKINT bit fields in SysTick Control and Status Register to enable interrupt
			return claimed;	
		}else{
			SYST_CSR|=1<<1;			//Set TICKINT bit fields in SysTick Control and Status Register to enable interrupt
			return claimed;
		}
	}else{
		SYST_CSR|=1<<1;						//Set TICKINT bit fields in SysTick Control and Status Register to enable interrupt
		return !available;
	}
}

void wait(void){
	SYST_CSR|=1<<0;			//Clear TICKINT bit fields in SysTick Control and Status Register to disable interrupt
	Tasks[runningTask].TASK_SP = __get_SP()+0x18; //Save SP RAM address for realignment when returning
	Tasks[runningTask].CONTEXT[6] = __get_LR();   //Save return address
	Tasks[runningTask].STATE=WAIT;
	schedule();
	if(runningTask!=-1){
		SYST_CSR|=1<<1;		//Set TICKINT bit fields in SysTick Control and Status Register to enable interrupt
		Tasks[runningTask].TASK_INITIAL_ADDR();
	}else{
		volatile register u32b reg = main_sp;
		asm("MOV SP,%0"::"r" (reg):"sp");
		//We enable interrupts here because this line uses registers R2 and R3.
		SYST_CSR|=1<<1;			//Re-enable SysTick interrupt to allow interruptions after pending task is activated
		reg=main_pc;
		__ASM volatile ("MOV R7, SP\n");				//Update register R7, core uses it as reference to SP
		__ASM volatile ("MOV PC, %0\n" : "=r" (reg));	//Overwrite PC to go to return address of pending task
	}
}

void alarmSweep(void){
	SYST_CSR|=1<<0;			//Clear TICKINT bit fields in SysTick Control and Status Register to disable interrupt
	u8b n=0;
	do{
		if(Alarms[n].ENABLE){
			Alarms[n].CNT--;
			if(Alarms[n].CNT==0){
				Tasks[Alarms[n].TASK_ID].STATE=READY;
				if(Alarms[n].RECURRENT){
					Alarms[n].CNT=Alarms[n].RELOAD;
				}else{
					Alarms[n].ENABLE=FALSE;
				}
			}
		}		
	}while(n++<NUMBER_OF_ALARMS-1);
	schedule();
	SYST_CSR|=1<<1;		//Set TICKINT bit fields in SysTick Control and Status Register to enable interrupt
	if(runningTask!=-1) Tasks[runningTask].TASK_INITIAL_ADDR();
}

void saveContext(void){	
	SYST_CSR|=1<<0;			//Clear TICKINT bit fields in SysTick Control and Status Register to disable interrupt
	volatile register u32b reg;
	isr_sp = (u32b*) __get_SP()+0x4; 		
	__ASM volatile("MOV %0,%[isr_sp]\n" : "=r" (reg) : [isr_sp] "rm" (*isr_sp));
	if(runningTask!=-1){
		Tasks[runningTask].TASK_SP=reg;			//Save interrupted task´s SP address for realignment when returning
		isr_sp+=0x2;							//Two address offset to go back to context addresses
		//Save context to structure´s array////////////
		i=0;
		do{
			__ASM volatile("MOV %0,%[isr_sp]\n" : "=r" (reg) : [isr_sp] "rm" (*isr_sp));
			Tasks[runningTask].CONTEXT[i]=reg;			//Store context
			isr_sp+=0x1;								//Offset one memory address at a time
		}while(i++<CONTEXT_SIZE-1);
		///////////////////////////////////////////////
		Tasks[runningTask].INTERRUPTED = TRUE; 	//Set INTERRUPTED flag of the task
		Tasks[runningTask].STATE=READY;
	}
	SYST_CSR|=1<<1;			//Set TICKINT bit fields in SysTick Control and Status Register to enable interrupt
}

void restoreContext(void){
	volatile register u32b reg = Tasks[runningTask].TASK_SP;
	asm ("MOV SP, %0\n" : "=r" (reg));	//Return SP top to the desired task stack address
	reg=Tasks[runningTask].CONTEXT[6];						//Get PC context
	asm ("MOV R5, %0\n" : "=r" (reg));	//Load PC context into R5
	reg=Tasks[runningTask].CONTEXT[2];						//Get R2 context
	asm ("MOV R8, %0\n" : "=r" (reg));	//Load R2 context into R8
	reg=Tasks[runningTask].CONTEXT[0];						//Get R0 context
	asm ("MOV R0, %0\n" : "=r" (reg));	//Load R0 context into R0
	reg=Tasks[runningTask].CONTEXT[1];						//Get R1 context
	asm ("MOV R1, %0\n" : "=r" (reg));	//Load R1context into R1
	reg=Tasks[runningTask].CONTEXT[4];						//Get R12 context
	asm ("MOV R12, %0\n" : "=r" (reg));	//Load R12 context into R12
	reg=Tasks[runningTask].CONTEXT[5];						//Get LR context
	asm ("MOV LR, %0\n" : "=r" (reg));	//Load LR context into LR
	//We enable interrupts here so R3 does not get overwritten after the context has been restored
	SYST_CSR|=1<<1;									//Set TICKINT bit fields in SysTick Control and Status Register to enable interrupt
	reg=Tasks[runningTask].CONTEXT[3];						//Get R3 context
	asm ("MOV R3, %0\n" : "=r" (reg));	//Load R3 context into R3
	asm ("MOV R2, R8\n");				//Load R8 into R2 for context restoring
	asm ("MOV R7, SP\n");				//Update register R7, core uses it as reference to SP
	asm ("MOV PC, R5\n");				//Overwrite PC to go to return address of pending task
}

void OS_init(void){
	main_sp = __get_SP()+0x10;	
	main_pc = __get_LR();   //Save return address
	i=0;	//Reset counter
	do{
		if(Tasks[i].AUTOSTART) Tasks[i].STATE=READY;	//Set READY state to AUTOSTART tasks
	}while(i++<TASKS_LIMIT);
	schedule();
	SYST_RVR=16777;		//Reload value range 1 - 16,777,215, 1ms timebase
	SYST_CVR=0;			//Write to SYST_CVR to reset current counter value and clear CSR COUNTFLAG
	SYST_CSR|=7;		//Set CLKSOURCE to processor clock, enable interrupts and SysTick counter
	if(Tasks[runningTask].STATE==RUN) Tasks[runningTask].TASK_INITIAL_ADDR();
}

void activateTask(u8b index,bool isr){
	SYST_CSR|=0<<1;		//Clear TICKINT bit field in SysTick Control and Status Register to disable interrupt
	if(!isr){
		Tasks[runningTask].TASK_SP = __get_SP()+0x18; //Save SP RAM address for realignment when returning
		Tasks[runningTask].CONTEXT[6] = __get_LR();   //Save return address to assign to running task later
	}
	Tasks[runningTask].STATE=READY;			 //Return running task to READY state
	if(Tasks[index].STATE==IDLE) Tasks[index].STATE=READY;
	schedule();
	SYST_CSR|=1<<1;		//Set TICKINT bit fields in SysTick Control and Status Register to enable interrupt
	if(Tasks[runningTask].INTERRUPTED){					//Restore context in the task was interrupted
		Tasks[runningTask].INTERRUPTED=FALSE;			//Reset task´s INTERRUPTED flag
		restoreContext();
	}else{
		Tasks[runningTask].TASK_INITIAL_ADDR();
	}
}

void chainTask(u8b index){
	SYST_CSR|=0<<1;		//Clear TICKINT bit field in SysTick Control and Status Register to disable interrupt
	Tasks[runningTask].STATE=IDLE;
	Tasks[index].STATE=READY;
	schedule();
	if(Tasks[runningTask].INTERRUPTED){
		Tasks[runningTask].INTERRUPTED=FALSE;			//Reset task´s INTERRUPTED flag
		restoreContext();
	}
	SYST_CSR|=1<<1;		//Set TICKINT bit fields in SysTick Control and Status Register to enable interrupt
	if(Tasks[runningTask].STATE==RUN) Tasks[runningTask].TASK_INITIAL_ADDR();
}

void terminateTask(void){
	SYST_CSR|=0<<1;		//Clear TICKINT bit field in SysTick Control and Status Register to disable interrupt
	Tasks[runningTask].STATE=IDLE;
	i=0;
	schedule();
	if(runningTask!=-1){
		if(Tasks[runningTask].INTERRUPTED){					//Restore context in the task was interrupted
			Tasks[runningTask].INTERRUPTED=FALSE;			//Reset task´s INTERRUPTED flag
			restoreContext();
		}else{
			volatile register u32b reg = Tasks[runningTask].TASK_SP;
			__ASM volatile ("MOV SP, %0\n" : "=r" (reg));		//Return SP top to the desired task stack address
			//We enable interrupts here because this line uses registers R2 and R3.
			SYST_CSR|=1<<1;			//Re-enable SysTick interrupt to allow interruptions after pending task is activated
			reg=Tasks[runningTask].CONTEXT[6];
			__ASM volatile ("MOV R7, SP\n");				//Update register R7, core uses it as reference to SP
			__ASM volatile ("MOV PC, %0\n" : "=r" (reg));	//Overwrite PC to go to return address of pending task
		}
	}else{
		volatile register u32b reg = main_sp;
		asm("MOV SP,%0"::"r" (reg):"sp");
		//We enable interrupts here because this line uses registers R2 and R3.
		SYST_CSR|=1<<1;			//Re-enable SysTick interrupt
		reg=main_pc;
		__ASM volatile ("MOV R7, SP\n");				//Update register R7, core uses it as reference to SP
		__ASM volatile ("MOV PC, %0\n" : "=r" (reg));	//Overwrite PC to go to return address of pending task
	}
}

void SysTick_Handler(void){
	saveContext();
	SYST_CVR=0;	//Write to SYST_CVR to reset current counter value and clear CSR COUNTFLAG
	alarmSweep();
}
//////////////////////////////////////////////////////////////////////////////////////////////
