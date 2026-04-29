#ifndef _SYSCALL_H
#define _SYSCALL_H

#include "handler.h"
enum {
  SYS_WRITE = 0,
  SYS_SLEEP,
  SYS_EXIT,
  SYS_WAIT,
  SYS_OPEN,
  SYS_CLOSE,
  SYS_GET_SIZE,
  SYS_READ,
  SYS_FORK,
  SYS_EXEC,
  SYS_KB_READ,
  SYS_READ_ROOT,
  SYS_GET_CPUID,
  SYS_GET_PID,
  SYS_GET_PS,
  SYS_MAX  // 自動計算總數
};
typedef int (*SYSTEMCALL)(int64_t* argptr);
void init_system_call(void);
void system_call(struct TrapFrame* tf);

#endif