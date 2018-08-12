#include "derivative.h"
#include "datatypes.h"
#include "os.h"

u8b runningTask;

//Helper methods

__attribute__( ( always_inline ) ) __STATIC_INLINE uint32_t __get_LR(){ 
  register uint32_t result; 
  __ASM volatile ("MOV %0, LR\n" : "=r" (result) ); 
  return(result); 
}

//

void OS_init(){
	//Initial scheduler///////////////////////////////////////
	u8b hpt=0;		//Highest Priority Task index
	u8b i=0;
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

void activateTask(u8b index){
	u32b return_addr = __get_LR()-1;		//Save return address to assign to running task later
	if((Tasks[index].STATE!=READY)&&(Tasks[index].STATE!=WAIT)) Tasks[index].STATE=READY;
	
	//Scheduler////////////////////////////////////////////
	u8b hpt=0;		//Highest Priority Task index
	u8b i=0;
	do{
		Tasks[runningTask].RETURN_ADDR=return_addr;		//Assign return address for running task
		Tasks[runningTask].STATE=READY;					//Return running task to READY state
		if((Tasks[i].PRIORITY > Tasks[hpt].PRIORITY)&&(Tasks[i].STATE==READY&&Tasks[hpt].STATE==READY)) hpt=i;
		i++;
	}while(i<NUMBER_OF_TASKS);
	///////////////////////////////////////////////////////
	
	Tasks[hpt].STATE=RUN;
	runningTask=hpt;
	Tasks[hpt].TASK_INITIAL_ADDR();
}

void chainTask(u8b index){
	
}

void terminateTask(){
	Tasks[runningTask].STATE=IDLE;			//Change running task state to IDLE
	bool pendingTasks=FALSE;
	
	//Scheduler////////////////////////////////////////////
	u8b hpt=0;		//Highest Priority Task index
	u8b i=0;
	do{
		if(Tasks[i].STATE==READY && !pendingTasks) pendingTasks=TRUE;
		if((Tasks[i].PRIORITY > Tasks[hpt].PRIORITY)&&(Tasks[i].STATE==READY&&Tasks[hpt].STATE==READY)) hpt=i;
		i++;
	}while(i<NUMBER_OF_TASKS);
	///////////////////////////////////////////////////////
	
	if(pendingTasks){
		Tasks[hpt].STATE=RUN;
		runningTask=hpt;
		(void) Tasks[hpt].RETURN_ADDR;
	}
}

