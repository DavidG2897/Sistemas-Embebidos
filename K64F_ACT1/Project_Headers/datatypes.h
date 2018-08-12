#ifndef DATATYPES_H_
#define DATATYPES_H_

#define TASKS_LIMIT 10
#define NUMBER_OF_TASKS 3
#define __ASM __asm 	/*!< asm keyword for GNU Compiler */ 
#define __INLINE inline /*!< inline keyword for GNU Compiler */ 
#define __STATIC_INLINE static inline 
#define TRUE 1
#define FALSE 0

typedef unsigned char bool;
typedef unsigned char u8b;
typedef unsigned long u32b;
enum STATES {IDLE,READY,RUN,WAIT};
struct{
	u8b PRIORITY;
	bool AUTOSTART;
	u32b RETURN_ADDR;
	void (*TASK_INITIAL_ADDR)();
	enum STATES STATE;
} Tasks[TASKS_LIMIT];

#endif /* DATATYPES_H_ */
