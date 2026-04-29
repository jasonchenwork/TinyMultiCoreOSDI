#ifndef _LIB_H
#define _LIB_H

#include "stdbool.h"
#include "stdint.h"

#if 0
typedef unsigned long long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
#endif

typedef struct {
  volatile uint64_t lock;
} spinlock_t;

struct List {
  struct List* next;
};

struct HeadList {
  struct List* next;
  struct List* tail;
};

struct ProcessState {
  int pid;
  int cpu_id;
  char name[8];
} __attribute__((packed));

void delay(uint64_t value);
void out_word(uint64_t addr, uint32_t value);
uint32_t in_word(uint64_t addr);

void memset(void* dst, int value, unsigned int size);
void memcpy(void* dst, void* src, unsigned int size);
void memmove(void* dst, void* src, unsigned int size);
int memcmp(void* src1, void* src2, unsigned int size);
unsigned char get_el(void);
uint64_t getcpuid(void);

void append_list_tail(struct HeadList* list, struct List* item);
struct List* remove_list_head(struct HeadList* list);
bool is_list_empty(struct HeadList* list);
struct List* remove_list(struct HeadList* list, int wait);
int get_list_count(struct HeadList* list);
void spin_init(spinlock_t* lock);
void spin_lock(spinlock_t* lock);
void spin_unlock(spinlock_t* lock);

#endif