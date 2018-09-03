#ifndef OS_H_
#define OS_H_
#include "datatypes.h"

void OS_init(void);
void activateTask(u8b, bool);
void saveContext(void);
void chainTask(u8b);
void terminateTask(void);
void createTask(u8b,u8b,bool,void*);
void createAlarm(u8b,u8b,u32b,bool,bool);
void createMailbox(u8b,void*,void*);
bool writeMailbox(u8b,u32b);		//Return 1 if write unsuccesfull, return 0 otherwise
u32b readMailbox(u8b,u32b*);		//Return 1 if read unsuccesfull, return 0 otherwise
void wait(void);					//Sends currently running task to WAIT state

#endif /* OS_H_ */
