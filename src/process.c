#include "process.h"

#include "debug.h"
#include "lib.h"
#include "memory.h"
#include "print.h"
#include "stddef.h"

static struct Process process_table[MAXCORES][NUM_PROC];
static int pid_num = 4;
static struct ProcessControl pc[MAXCORES];
void pstart(void);

spinlock_t pid_lock = {0};
spinlock_t process_lock = {0};

struct ProcessControl* get_pc(void) { return &pc[getcpuid()]; }
struct ProcessControl* get_cpuidpc(int cur_id) { return &pc[cur_id]; }

static int find_least_busy_core(void) {
  int min_tasks = 999;
  int target_core = 0;
  return 0;

  for (int i = 0; i < MAXCORES; i++) {
    // 假設您的 HeadList 有一個 count 欄位記錄長度
    // 或者手動遍歷 list 計算長度
    int count = get_list_count(&pc[i].ready_list);
    if (count < min_tasks) {
      min_tasks = count;
      target_core = i;
    }
  }
  return target_core;
}

static struct Process* find_unused_process(int cpu_id) {
  // debug_spinlock(__FILE__, __LINE__);
  // spin_lock(&process_lock);
  struct Process* process = NULL;

  for (int i = 1; i < NUM_PROC; i++) {
    if (process_table[cpu_id][i].state == PROC_UNUSED) {
      process = &process_table[cpu_id][i];
      break;
    }
  }

  // debug_spinunlock(__FILE__, __LINE__);
  // spin_unlock(&process_lock);
  return process;
}
void k_strcpy(char* dest, const char* src) {
  while (*src != '\0') {
    *dest = *src;
    dest++;
    src++;
  }
  *dest = '\0';  // 補上結尾符號
}
void init_idle_process() {
#if 1

  for (int i = 0; i < MAXCORES; i++) {
    struct Process* process = &process_table[i][0];
    process->state = PROC_RUNNING;
    process->pid = i;
    // k_strcpy(process->name, );
    memcpy(process->name, "idle", 8);
    // printk("[Core %d] name: %s\n", i, process->name);

    // 核心 Idle 應該指向內核的基礎頁表 (PGD_TTBR0)
    // 這裡直接用彙編定義的符號位址，而不是 read_pgd()，更安全
    extern char pgd_ttbr0;
    process->page_map = (uint64_t)V2P(&pgd_ttbr0);

    pc[i].current_process = process;
    ///  printk("[Core %d] Idle process initialized with PID 0 and PageMap
    ///  0x%x\n", i, process->page_map);
  }
  //  printk("All idle processes initialized successfully!\n");
  // [重要] 確保 Core 0 的初始化對所有核心可見
  __asm__ volatile("dsb ish; isb");

#else
  struct Process* process;
  struct ProcessControl* process_control;
  int curcore = getcpuid();

  process = &process_table[curcore];  // find_unused_process();
  // ASSERT(process == &process_table[curcore]);

  process->state = PROC_RUNNING;
  process->pid = 0;
  process->page_map = P2V(read_pgd());

  process_control = get_pc();
  process_control->current_process = process;
  printk("[Core %d] Idle process initialized with PID 0 and PageMap 0x%x\n",
         curcore, process->page_map);
#endif
}

static struct Process* alloc_new_process(int cur_id) {
  struct Process* process;
  // int curcore = getcpuid();
  if (cur_id == -1) {
    process = find_unused_process(getcpuid());
  } else {
    process = find_unused_process(cur_id);
  }

  if (process == NULL) {
    return NULL;
  }
  spin_lock(&pid_lock);
  process->stack = (uint64_t)kalloc();
  ASSERT(process->stack != 0);
  memset((void*)process->stack, 0, PAGE_SIZE);

  process->state = PROC_INIT;

  process->pid = pid_num++;

  process->context =
      process->stack + PAGE_SIZE - sizeof(struct TrapFrame) - 12 * 8;
  *(uint64_t*)(process->context + 11 * 8) = (uint64_t)pstart;  // trap_return;
  process->tf = (struct TrapFrame*)(process->stack + PAGE_SIZE -
                                    sizeof(struct TrapFrame));
  process->tf->elr = 0x400000;
  process->tf->sp0 = 0x400000 + PAGE_SIZE;
  process->tf->spsr = 0;

  process->page_map = (uint64_t)kalloc();

  int curcore = getcpuid();

  printk(
      "alloc_new_process [Core %d] Fork: PID %d cloning PageMap: 0x%x Stack: "
      "0x%x\n",
      curcore, process->pid, process->page_map, process->stack);

  ASSERT(process->page_map != 0);
  memset((void*)process->page_map, 0, PAGE_SIZE);
  spin_unlock(&pid_lock);
  return process;
}

static void init_user_process(void) {
  struct Process* process;
  struct ProcessControl* process_control;
  struct HeadList* list;

  process = alloc_new_process(-1);
  ASSERT(process != NULL);

  ASSERT(setup_uvm(process, "INIT.BIN"));

  memcpy(process->name, "init", 8);

  process_control = get_pc();
  list = &process_control->ready_list;

  process->state = PROC_READY;
  append_list_tail(list, (struct List*)process);
}
static void init_test_process(void) {
  struct Process* process;
  struct ProcessControl* process_control;
  struct HeadList* list;

  process = alloc_new_process(-1);
  ASSERT(process != NULL);

  ASSERT(setup_uvm(process, "TEST2.BIN"));

  memcpy(process->name, "test2", 8);
  process_control = get_pc();
  list = &process_control->ready_list;

  process->state = PROC_READY;
  append_list_tail(list, (struct List*)process);
}
static void init_test3_process(void) {
  struct Process* process;
  struct ProcessControl* process_control;
  struct HeadList* list;

  process = alloc_new_process(-1);
  ASSERT(process != NULL);

  ASSERT(setup_uvm(process, "TEST3.BIN"));
  memcpy(process->name, "test3", 8);
  process_control = get_pc();
  list = &process_control->ready_list;

  process->state = PROC_READY;
  append_list_tail(list, (struct List*)process);
}
static void init_test4_process(void) {
  struct Process* process;
  struct ProcessControl* process_control;
  struct HeadList* list;

  process = alloc_new_process(-1);
  ASSERT(process != NULL);

  ASSERT(setup_uvm(process, "TEST4.BIN"));
  memcpy(process->name, "test4", 8);

  process_control = get_pc();
  list = &process_control->ready_list;

  process->state = PROC_READY;
  append_list_tail(list, (struct List*)process);
}
void init_process(void) {
  int curcore = getcpuid();
  // printk("Core %d initializing process...read_pgd(): 0x%x\n", curcore,
  //        read_pgd());
  //

  // init_idle_process(curcore);
  if (curcore == 0) {
    init_idle_process();
    init_test4_process();
    init_user_process();

    // init_test3_process();

    printk("Core %d initialized process successfully!\n", curcore);
  } else {
    // init_test3_process();
    init_test4_process();
    // init_test4_process();
    //  init_test_process();
  }
}

static void switch_process(struct Process* prev, struct Process* current) {
  switch_vm(current->page_map);
  swap(&prev->context, current->context);
}

static void schedule(void) {
  struct Process* prev_proc;
  struct Process* current_proc;
  struct ProcessControl* process_control;
  struct HeadList* list;
  int curcore = getcpuid();

  process_control = get_pc();
  list = &process_control->ready_list;
  // spin_lock(&process_lock);

  prev_proc = process_control->current_process;

  //

  if (is_list_empty(list)) {
    current_proc = &process_table[curcore][0];
  } else {
    current_proc = (struct Process*)remove_list_head(list);
  }

#if 0
  // Debug: 看看是誰被中斷強迫放棄 CPU
  if (current_proc->pid != 0) {
    printk("Yielding PID prev:%d cur:%d to tail\n", prev_proc->pid,
           current_proc->pid);
  }
#endif
  current_proc->state = PROC_RUNNING;
  if (current_proc == NULL || prev_proc == current_proc) {
    return;
  }
  // spin_unlock(&process_lock);
  process_control->current_process = current_proc;
#if 0
  printk("[core :%d]Switching process: prev PID %d -> current PID %d\r\n",
         curcore, prev_proc->pid, current_proc->pid);
#endif

  // --- 關鍵：防止自我切換 ---

  switch_process(prev_proc, current_proc);
  // spin_unlock(&process_lock);
}

void yield(void) {
  struct Process* process;
  struct ProcessControl* process_control;
  struct HeadList* list;

  process_control = get_pc();
  // printk("[Core %d] ", getcpuid());
  // debug_spinlock(__FILE__, __LINE__);
  spin_lock(&process_lock);
  list = &process_control->ready_list;

  if (is_list_empty(list)) {
    // printk("yield  is_list_empty\n");
    //  printk("[Core %d] is empty ", getcpuid());
    //  debug_spinunlock(__FILE__, __LINE__);
    spin_unlock(&process_lock);
    return;
  }

  process = process_control->current_process;
  // printk("Process %d yielding CPU\r\n", process->pid);

  process->state = PROC_READY;

  if (process->pid >= 4) {
    append_list_tail(list, (struct List*)process);
  }

  schedule();
  // printk("[Core %d] ", getcpuid());
  // debug_spinunlock(__FILE__, __LINE__);
  spin_unlock(&process_lock);
}

void sleep(int wait) {
  spin_lock(&process_lock);
  struct Process* process;
  struct ProcessControl* process_control;
  process_control = get_pc();
  process = process_control->current_process;
  process->state = PROC_SLEEP;
  process->wait = wait;

  append_list_tail(&process_control->wait_list, (struct List*)process);

  schedule();
  spin_unlock(&process_lock);
}

void wake_up(int wait) {
  spin_lock(&process_lock);  // 保護全域清單
#if 1
  for (int i = 0; i < MAXCORES; i++) {
    struct HeadList* wait_l = &pc[i].wait_list;
    struct Process* p = (struct Process*)remove_list(wait_l, wait);

    while (p != NULL) {
      p->state = PROC_READY;
      // 喚醒後放回該核心的 ready_list
      append_list_tail(&pc[i].ready_list, (struct List*)p);
      p = (struct Process*)remove_list(wait_l, wait);
    }
  }
#else
  struct Process* process;
  struct ProcessControl* process_control;
  struct HeadList* ready_list;
  struct HeadList* wait_list;
  process_control = get_pc();
  ready_list = &process_control->ready_list;
  wait_list = &process_control->wait_list;

  process = (struct Process*)remove_list(wait_list, wait);

  while (process != NULL) {
    process->state = PROC_READY;
    //
    append_list_tail(ready_list, (struct List*)process);

    process = (struct Process*)remove_list(wait_list, wait);
  }
#endif
  spin_unlock(&process_lock);
}

void exit(void) {
  struct Process* process;
  struct ProcessControl* process_control;
  process_control = get_pc();
  process = process_control->current_process;
  process->state = PROC_KILLED;
  process->wait = process->pid;
  spin_lock(&process_lock);  // 先拿鎖
  append_list_tail(&process_control->kill_list, (struct List*)process);
  spin_unlock(&process_lock);
  wake_up(-3);
  spin_lock(&process_lock);  // 先拿鎖
  schedule();
}

void wait(int pid) {
  struct Process* process;
  struct ProcessControl* process_control;
  struct HeadList* list;
  process_control = get_pc();
  list = &process_control->kill_list;

  while (1) {
    spin_lock(&process_lock);  // 保護全域清單
    if (!is_list_empty(list)) {
      process = (struct Process*)remove_list(list, pid);

      if (process != NULL) {
        ASSERT(process->state == PROC_KILLED);

        kfree(process->stack);
        free_vm(process->page_map);

        for (int i = 0; i < 100; i++) {
          if (process->file[i] != NULL) {
            process->file[i]->fcb->count--;
            process->file[i]->count--;

            if (process->file[i]->count == 0) {
              process->file[i]->fcb = NULL;
            }
          }
        }

        memset(process, 0, sizeof(struct Process));

        spin_unlock(&process_lock);
        break;
      }
    }

    spin_unlock(&process_lock);
    sleep(-3);
  }
}
int getProcessState(struct ProcessState* ps) {
  spin_lock(&process_lock);
  int cnt = 0;
  for (int i = 0; i < MAXCORES; i++) {
    for (int j = 0; j < NUM_PROC; j++) {
      if (process_table[i][j].state != PROC_UNUSED) {
        ps[cnt].pid = process_table[i][j].pid;
        ps[cnt].cpu_id = i;
        memcpy(ps[cnt].name, process_table[i][j].name, 8);
        cnt++;
      }
    }
  }

  spin_unlock(&process_lock);
  return cnt;
}

int fork(void) {
  struct ProcessControl* process_control;
  struct Process* process;
  struct Process* current_process;
  struct HeadList* list;
  struct ProcessControl* target_pc;
  int curcore = getcpuid();
  debug_spinlock(__FILE__, __LINE__);
  spin_lock(&process_lock);

  int target_core = find_least_busy_core();
  process_control = get_cpuidpc(target_core);

  current_process = process_control->current_process;
  list = &process_control->ready_list;

  process = alloc_new_process(target_core);

  if (process == NULL) {
    ASSERT(0);
    debug_spinunlock(__FILE__, __LINE__);
    spin_unlock(&process_lock);
    return -1;
  }

  printk("[Core %d] Fork: PID %d cloning PageMap: 0x%x\n", curcore,
         current_process->pid, current_process->page_map);

  bool res = copy_uvm(process->page_map, current_process->page_map, PAGE_SIZE);

  if (res == false) {
    ASSERT(0);
    debug_spinunlock(__FILE__, __LINE__);

    spin_unlock(&process_lock);
    return -1;
  }

  memcpy(process->file, current_process->file, 100 * sizeof(struct FileDesc*));

  memcpy(process->name,
         process->file[0] ? process->file[0]->fcb->name : "unknown", 8);

  for (int i = 0; i < 100; i++) {
    if (process->file[i] != NULL) {
      process->file[i]->count++;
      process->file[i]->fcb->count++;
    }
  }

  memcpy(process->tf, current_process->tf, sizeof(struct TrapFrame));
  // spin_lock(&process_lock);
  process->tf->x0 = 0;
  process->state = PROC_READY;

  // int target_core = process->pid % NUM_CORES;
  // target_pc = &pc[target_core];

  // spin_lock(&process_lock);
  append_list_tail(&process_control->ready_list, (struct List*)process);
  // spin_unlock(&process_lock);
  debug_spinunlock(__FILE__, __LINE__);
  spin_unlock(&process_lock);

  return process->pid;
}

int exec(struct Process* process, char* name) {
  int fd;
  uint32_t size;

  fd = open_file(process, name);
  if (fd == -1) {
    exit();
  }

  memset((void*)0x400000, 0, PAGE_SIZE);
  size = get_file_size(process, fd);
  size = read_file(process, fd, (void*)0x400000, size);
  if (size == 0xffffffff) {
    exit();
  }

  close_file(process, fd);

  memset(process->tf, 0, sizeof(struct TrapFrame));
  process->tf->elr = 0x400000;
  process->tf->sp0 = 0x400000 + PAGE_SIZE;
  process->tf->spsr = 0;

  return 0;
}
