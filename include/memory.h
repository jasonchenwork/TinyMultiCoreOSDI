#ifndef _MEMORY_H
#define _MEMORY_H

#include "lib.h"
#include "process.h"
#include "stdbool.h"
#include "stdint.h"

#define MAX_ORDER 9               // 最大階層 (2^9 頁 =  512MB)
#define MEM_SIZE 2 * 1024 * 1024  // 總共 2MB

#define LinkListMode 0
#define BuddyMode 1
#define MEMORYMANAGER BuddyMode

// 雙向鏈結串列節點
typedef struct list_node {
  struct list_node *prev, *next;
} list_node_t;

// 夥伴系統管理器
typedef struct {
  spinlock_t lock __attribute__((aligned(16)));
  list_node_t free_lists[MAX_ORDER];
  uintptr_t start_addr;

} buddy_allocator_t;

static buddy_allocator_t allocator;

struct Page {
  spinlock_t lock __attribute__((aligned(16)));
  struct Page* next;
};

#define KERNEL_BASE 0xffff000000000000
#define P2V(p) ((uint64_t)(p) + KERNEL_BASE)
#define V2P(v) ((uint64_t)(v) - KERNEL_BASE)

#define MEMORY_END P2V(0x30000000)
#define PAGE_SHIFT 21  // 21
#define PAGE_SIZE (2 * 1024 * 1024)

#define PA_UP(v) ((((uint64_t)v + PAGE_SIZE - 1) >> PAGE_SHIFT) << PAGE_SHIFT)
#define PA_DOWN(v) (((uint64_t)v >> PAGE_SHIFT) << PAGE_SHIFT)

#define PDE_ADDR(p) ((uint64_t)p & 0xfffffffff000)
#define PTE_ADDR(p) ((uint64_t)p & 0xffffffe00000)

#define ENTRY_V (1 << 0)
#define TABLE_ENTRY (1 << 1)
#define BLOCK_ENTRY (0 << 1)
#define ENTRY_ACCESSED (1 << 10)
#define NORMAL_MEMORY (1 << 2)
#define DEVICE_MEMORY (0 << 2)
#define USER (1 << 6)

/*
 *
 * Block Descriptor Regorms (Level 1 & 2)
 *
 */

/*
 * 描述符類型 (Level 0-2)
 * Bits [1:0]
 */
#define TD_TYPE_INVALID 0x0  // 無效或未映射
#define TD_TYPE_BLOCK 0x1    // Block Descriptor (僅限 Level 1 & 2)
#define TD_TYPE_TABLE 0x3    // Table Descriptor (指向下一層)
#define TD_TYPE_PAGE 0x3     // Page Descriptor (僅限 Level 3)

/*
 * 存取限制與 Lower Attributes
 * Bits [11:2]
 */
// AttrIndx [4:2] : 指向 MAIR_EL1 暫存器的索引
#define TD_ATTR_INDEX(idx) ((idx) << 2)
#define TD_ATTR_NORMAL TD_ATTR_INDEX(1)
#define TD_ATTR_DEVICE TD_ATTR_INDEX(0)

// NS [5] : Non-Secure bit (1 為非安全)
#define TD_NS (1ULL << 5)

// AP [7:6] : Access Permissions
#define TD_AP_RW_EL1 (0ULL << 6)  // 核心可讀寫 (EL1)
#define TD_AP_RW_ALL (1ULL << 6)  // 核心與用戶皆可讀寫 (EL1 & EL0)
#define TD_AP_RO_EL1 (2ULL << 6)  // 核心唯讀
#define TD_AP_RO_ALL (3ULL << 6)  // 全域唯讀

// SH [9:8] : Shareability
#define TD_SH_NON_SHARE (0ULL << 8)
#define TD_SH_OUTER_SHARE (2ULL << 8)
#define TD_SH_INNER_SHARE (3ULL << 8)

// AF [10] : Access Flag (避免存取例外)
#define TD_AF (1ULL << 10)

// nG [11] : non-Global (用於 ASID 區分不同 Process 的快取)
#define TD_NON_GLOBAL (1ULL << 11)

/*
 * Upper Attributes
 * Bits [63:52]
 */
// Privileged Execute-Never (EL1 不可執行)
#define TD_PXN (1ULL << 53)
// Unprivileged Execute-Never (EL0 不可執行)
#define TD_UXN (1ULL << 54)
// Software Reserved (這幾個位元可供 OS 自由使用)
#define TD_SW_RESERVED (0b1111ULL << 55)

/*
 *
 */
#define BOOT_NORMAL_ATTR \
  (TD_AF | TD_SH_INNER_SHARE | TD_ATTR_NORMAL | TD_TYPE_BLOCK)
#define BOOT_DEVICE_ATTR (TD_AF | TD_ATTR_DEVICE | TD_TYPE_BLOCK)
#define USER_DATA_ATTR \
  (TD_AF | TD_SH_INNER_SHARE | TD_ATTR_NORMAL | TD_TYPE_BLOCK | TD_AP_RW_ALL)

// RPi3B 記憶體佈局
#define RAM_MAX_ADDR 0x34000000
#define PERIPHERAL_BASE 0x3F000000
#define PERIPHERAL_END 0x40000000
#define LOCAL_PERIPH_BASE 0x40000000
#define LOCAL_PERIPH_END 0x41000000

void* kalloc(void);
void kfree(uint64_t v);
void init_memory(void);
bool map_page(uint64_t map, uint64_t v, uint64_t pa, uint64_t attribute);
void switch_vm(uint64_t map);
bool setup_uvm(struct Process* process, char* file_name);
void free_page(uint64_t map, uint64_t vstart);
void free_vm(uint64_t map);
uint64_t read_pgd(void);
bool copy_uvm(uint64_t dst_map, uint64_t src_map, int size);
void setup_vm2(void);
#endif
