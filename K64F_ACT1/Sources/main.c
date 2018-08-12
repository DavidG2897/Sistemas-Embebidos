#include "derivative.h" /* include peripheral declarations */
#include "os.h"
#include "datatypes.h"

char c;

void taskA(){
	char a = 5;
	char b = 10;
	activateTask(1);		//User-defined index for task B
	c = a + b;
	terminateTask();
}

void taskB(){
	c++;
	terminateTask();
}
void taskC(){
	c*=2;
}

int main(void){
	
	//volatile Task tasks[number_of_tasks];		//User defined tasks size
	
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
