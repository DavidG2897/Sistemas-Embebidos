#ifndef OS_H_
#define OS_H_
#include "datatypes.h"

void OS_init(void);
void activateTask(u8b,bool);
void saveContext(void);
void chainTask(u8b);
void terminateTask(void);
__attribute__( ( always_inline ) ) __STATIC_INLINE uint32_t __get_LR(void);
__attribute__( ( always_inline ) ) __STATIC_INLINE uint32_t __get_SP(void);

#endif /* OS_H_ */
