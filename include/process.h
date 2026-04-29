#ifndef _PROCESS_H
#define _PROCESS_H

#include "file.h"
// #include "cpio.h"
#include "handler.h"
#include "lib.h"

struct Process {
  struct List* next;
  int pid;
  int state;
  int wait;
  char name[8];
  uint64_t context;
  uint64_t page_map;
  uint64_t stack;
  struct FileDesc* file[100];
  struct TrapFrame* tf;
};

struct ProcessControl {
  struct Process* current_process;
  struct HeadList ready_list;
  struct HeadList wait_list;
  struct HeadList kill_list;
};

#define STACK_SIZE (2 * 1024 * 1024)
#define NUM_PROC 30
#define MAXCORES 4

typedef enum {
  PROC_UNUSED = 0,
  PROC_INIT = 1,
  PROC_RUNNING = 2,
  PROC_READY = 3,
  PROC_SLEEP = 4,
  PROC_KILLED = 5
} proc_state;

void init_process(void);
struct ProcessControl* get_pc(void);
void yield(void);
void swap(uint64_t* prev, uint64_t next);
void trap_return(void);
void sleep(int wait);
void wake_up(int wait);
void exit(void);
void wait(int pid);
int fork(void);
int exec(struct Process* process, char* name);
void init_idle_process();
int getProcessState(struct ProcessState* ps);
#endif