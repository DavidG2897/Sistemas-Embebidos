#include "derivative.h"
#include "datatypes.h"
#include "os.h"

u8b runningTask,hpt,i;
bool pendingTasks;
u32b reg_C;
u32b *isr_sp;

//Helper methods

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

//

void OS_init(){
	//Initial scheduler///////////////////////////////////////
	hpt=0;		//Highest Priority Task index
	i=0;
	do{
		if(Tasks[i].AUTOSTART){
			Tasks[i].STATE=READY;															//These way we make sure it
			if((Tasks[i].PRIORITY > Tasks[hpt].PRIORITY)&&Tasks[hpt].STATE==READY) hpt=i;	//saves the HPT with AUTOSTART
		}
		i++;
	}while(i<NUMBER_OF_TASKS);
	//////////////////////////////////////////////////
	Tasks[hpt].STATE=RUN;
	runningTask=hpt;
	Tasks[hpt].TASK_INITIAL_ADDR();
}

/*
 * Context indexes:
 * 	0: R0
 * 	1: R1
 * 	2: R2
 * 	3: R3
 * 	4: R12
 * 	5: LR
 * 	6: PC - Return address
 * 	7: XPSR
 */
void saveContext(void){	
	volatile register u32b reg;
	isr_sp = (u32b*) __get_SP()+0x4; 		
	__ASM volatile("MOV %0,%[isr_sp]\n" : "=r" (reg) : [isr_sp] "rm" (*isr_sp));
	Tasks[runningTask].SP_FOR_TASK=reg;		//Save SP RAM address for realignment when returning
	isr_sp+=0x2;
	i=0;
	do{
		__ASM volatile("MOV %0,%[isr_sp]\n" : "=r" (reg) : [isr_sp] "rm" (*isr_sp));
		Tasks[runningTask].CONTEXT[i]=reg;
		isr_sp+=0x1;
		i++;
	}while(i<CONTEXT_SIZE);
	Tasks[runningTask].RETURN_ADDR = Tasks[runningTask].CONTEXT[6];
	Tasks[runningTask].INTERRUPTED = TRUE;
}

void activateTask(u8b index,bool isr){
	if(!isr){
		Tasks[runningTask].SP_FOR_TASK = __get_SP()+0x18; //Save SP RAM address for realignment when returning
		Tasks[runningTask].RETURN_ADDR = __get_LR(); 	//Save return address to assign to running task later
	}
	Tasks[runningTask].STATE=READY;			 //Return running task to READY state

	if((Tasks[index].STATE!=READY)&&(Tasks[index].STATE!=WAIT)) Tasks[index].STATE=READY;

	//Scheduler////////////////////////////////////////////
	hpt=0;		//Highest Priority Task index
	i=0;
	do{
		if((Tasks[i].PRIORITY > Tasks[hpt].PRIORITY)&&(Tasks[i].STATE==READY&&Tasks[hpt].STATE==READY)) hpt=i;
		i++;
	}while(i<NUMBER_OF_TASKS);
	///////////////////////////////////////////////////////

	Tasks[hpt].STATE=RUN;
	runningTask=hpt;
	Tasks[hpt].TASK_INITIAL_ADDR();
}

void chainTask(u8b index){
	Tasks[runningTask].STATE=IDLE;
	Tasks[index].STATE=READY;

	//Scheduler////////////////////////////////////////////
	hpt=0;		//Highest Priority Task index
	i=0;
	do{
		if((Tasks[i].PRIORITY > Tasks[hpt].PRIORITY)&&(Tasks[i].STATE==READY&&Tasks[hpt].STATE==READY)) hpt=i;
		i++;
	}while(i<NUMBER_OF_TASKS);
	///////////////////////////////////////////////////////

	runningTask=hpt;
	Tasks[hpt].TASK_INITIAL_ADDR();
}

void terminateTask(){
	Tasks[runningTask].STATE=IDLE;			//Change running task state to IDLE
	pendingTasks=FALSE;

	//Scheduler////////////////////////////////////////////
	hpt=0;		//Highest Priority Task index
	i=0;
	do{
		if(Tasks[i].STATE==READY && !pendingTasks) pendingTasks=TRUE;
		if((Tasks[i].PRIORITY > Tasks[hpt].PRIORITY)&&(Tasks[i].STATE==READY&&Tasks[hpt].STATE==READY)) hpt=i;
		i++;
	}while(i<NUMBER_OF_TASKS);
	///////////////////////////////////////////////////////

	if(pendingTasks){
		Tasks[hpt].STATE=RUN;
		runningTask=hpt;
		//Restore context
		volatile register u32b sp_reg = Tasks[hpt].SP_FOR_TASK;
		__ASM volatile ("MOV SP, %0\n" : "=r" (sp_reg));		//Return SP top to the desired task stack address
		/*
		 * Context indexes:
		 * 	0: R0
		 * 	1: R1
		 * 	2: R2
		 * 	3: R3
		 * 	4: R12
		 * 	5: LR
		 * 	6: PC - Return address
		 * 	7: XPSR
		 */
		if(Tasks[hpt].INTERRUPTED){
			Tasks[hpt].INTERRUPTED=FALSE;
			volatile register u32b pc_reg=Tasks[hpt].CONTEXT[6];		//Get PC context
			__ASM volatile ("MOV R5, %0\n" : "=r" (pc_reg));			//Load PC into R5
			volatile register u32b r2_reg=Tasks[hpt].CONTEXT[2];		//Get R2 context
			__ASM volatile ("MOV R8, %0\n" : "=r" (r2_reg));			//Load R2 into R8
			
			volatile register u32b reg=Tasks[hpt].CONTEXT[0];	//Get R0 context
			__ASM volatile ("MOV R0, %0\n" : "=r" (reg));		//Load R0 context into R0
			reg=Tasks[hpt].CONTEXT[1];							//Get R1 context
			__ASM volatile ("MOV R1, %0\n" : "=r" (reg));		//Load R1context into R1
			reg=Tasks[hpt].CONTEXT[4];							//Get R12 context
			__ASM volatile ("MOV R12, %0\n" : "=r" (reg));		//Load R12 context into R12
			reg=Tasks[hpt].CONTEXT[5];							//Get LR context
			__ASM volatile ("MOV LR, %0\n" : "=r" (reg));		//Load LR context into LR
			reg=Tasks[hpt].CONTEXT[3];							//Get R3 context
			__ASM volatile ("MOV R3, %0\n" : "=r" (reg));		//Load R3 context into R3
			__ASM volatile ("MOV R2, R8\n");
			__ASM volatile ("MOV R7, SP\n");	
			__ASM volatile ("MOV PC, R5\n");
		}else{
			volatile register u32b pc_reg=Tasks[hpt].RETURN_ADDR;
			__ASM volatile ("MOV R7, SP\n");					//Update register R7, core uses it as reference to SP
			__ASM volatile ("MOV PC, %0\n" : "=r" (pc_reg));	//Overwrite PC to go to return address of pending task
		}
	}
}
