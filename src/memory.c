#include "memory.h"

#include "debug.h"
#include "file.h"
// #include "cpio.h"
#include "lib.h"
#include "print.h"
#include "stdbool.h"
#include "stddef.h"
#include "stdint.h"
// #include "tmmu.h"

static struct Page free_memory;
extern char end;

void list_init(list_node_t* head) {
  head->next = head;
  head->prev = head;
}

void list_add(list_node_t* head, list_node_t* node) {
  node->next = head->next;
  node->prev = head;
  head->next->prev = node;
  head->next = node;
}

void list_del(list_node_t* node) {
  node->prev->next = node->next;
  node->next->prev = node->prev;
}
bool list_empty(list_node_t* head) { return head->next == head; }

void buddy_init(uintptr_t start, size_t size) {
  allocator.start_addr = start;

  // 1. 初始化所有空閒鏈結串列
  for (int i = 0; i < MAX_ORDER; i++) {
    list_init(&allocator.free_lists[i]);
  }
#if 1
  uintptr_t current = start;
  size_t remaining = size;

  while (remaining >= (1UL << 21)) {  // 至少還剩 2MB
    for (int i = MAX_ORDER - 1; i >= 0; i--) {
      size_t block_size = 1UL << (i + 21);

      // 如果剩下的空間夠大，且「目前位址」對齊了這個 Order 的邊界
      if (remaining >= block_size && (current % block_size == 0)) {
        list_add(&allocator.free_lists[i], (list_node_t*)current);

        current += block_size;
        remaining -= block_size;
        printk("初始化: 分配 0x%x 到 Order %x\n", current - block_size, i);
        // 成功放入一個「當下能拿到的最大區塊」後
        // 必須立刻跳出 for 迴圈，讓 while 從 MAX_ORDER-1 重新檢查新位址
        goto next_block;
      }
    }
  next_block:;
  }
#else
  uintptr_t current_addr = start;
  size_t remaining_size = size;

  // 2. 核心邏輯：將 size 拆解為多個 Power of 2 區塊
  // 由大到小檢查每個 Order 是否能容納剩下的空間
  for (int i = MAX_ORDER - 1; i >= 0; i--) {
    size_t order_size = 1UL << (i + PAGE_SHIFT);  // 該 Order 對應的位元組大小

    while (remaining_size >= order_size) {
      // 確保地址對齊該 Order (這在 ARMv8 MMU 映射中極度重要)
      if ((current_addr & (order_size - 1)) == 0) {
        list_add(&allocator.free_lists[i], (list_node_t*)current_addr);

        current_addr += order_size;
        remaining_size -= order_size;
        printk("初始化: 分配 0x%x 到 Order %x\n", current_addr - order_size, i);
      } else {
        // 如果地址沒對齊這個大的 Order，就跳過讓更小的 Order 去處理
        break;
      }
    }
  }

#endif
}

void load_pgd(uint64_t map);
/* TTBR1 - 核心空間頁表 (Kernel Space) */
extern uint64_t pgd_ttbr1[512] __attribute__((aligned(4096)));
;
extern uint64_t pud_ttbr1[512] __attribute__((aligned(4096)));
;
extern uint64_t pmd_ttbr1[512] __attribute__((aligned(4096)));
;
extern uint64_t pmd2_ttbr1[512] __attribute__((aligned(4096)));
;

/* TTBR0 - 用戶空間頁表 (User Space) */
extern uint64_t pgd_ttbr0[512] __attribute__((aligned(4096)));
;
extern uint64_t pud_ttbr0[512] __attribute__((aligned(4096)));
;
extern uint64_t pmd_ttbr0[512] __attribute__((aligned(4096)));
;

void setup_vm2(void) {
  uint64_t pgd1_pa, pud1_pa, pmd1_pa, pmd2_pa;
  uint64_t pgd0_pa, pud0_pa, pmd0_pa;

  __asm__ volatile("adr %0, pgd_ttbr1" : "=r"(pgd1_pa));
  __asm__ volatile("adr %0, pud_ttbr1" : "=r"(pud1_pa));
  __asm__ volatile("adr %0, pmd_ttbr1" : "=r"(pmd1_pa));
  __asm__ volatile("adr %0, pmd2_ttbr1" : "=r"(pmd2_pa));

  __asm__ volatile("adr %0, pgd_ttbr0" : "=r"(pgd0_pa));
  __asm__ volatile("adr %0, pud_ttbr0" : "=r"(pud0_pa));
  __asm__ volatile("adr %0, pmd_ttbr0" : "=r"(pmd0_pa));

  // 2. 將物理位址轉為可寫入的指標
  volatile uint64_t* pgd1 = (volatile uint64_t*)pgd1_pa;
  volatile uint64_t* pud1 = (volatile uint64_t*)pud1_pa;
  volatile uint64_t* pmd1 = (volatile uint64_t*)pmd1_pa;
  volatile uint64_t* pmd2 = (volatile uint64_t*)pmd2_pa;
  volatile uint64_t* pgd0 = (volatile uint64_t*)pgd0_pa;
  volatile uint64_t* pud0 = (volatile uint64_t*)pud0_pa;
  volatile uint64_t* pmd0 = (volatile uint64_t*)pmd0_pa;

  // --- setup_kvm (TTBR1) ---

  // 3. PGD[0] = pud_pa | 0x3
  pgd1[0] = pud1_pa | TD_TYPE_TABLE;

  // 4. PUD[0] = pmd_pa | 0x3
  pud1[0] = pmd1_pa | TD_TYPE_TABLE;

  // 5. Loop 1: RAM 映射 (0x0 ~ 0x34000000)
  uint64_t pa = 0;
  // uint64_t attr_normal = (1ULL << 10) | (1ULL << 2) | 0x1;
  while (pa < RAM_MAX_ADDR) {
    pmd1[pa >> 21] = pa | BOOT_NORMAL_ATTR;
    pa += PAGE_SIZE;
  }

  // 6. Loop 2: UART/Device (0x3F000000 ~ 0x40000000)
  pa = 0x3f000000;
  // uint64_t attr_device = (1ULL << 10) | 0x1;
  while (pa < PERIPHERAL_END) {
    pmd1[pa >> 21] = pa | BOOT_DEVICE_ATTR;
    pa += PAGE_SIZE;
  }

  // 7. PUD[1] 指向 PMD2 並映射 Local Peripherals
  pud1[1] = pmd2_pa | TD_TYPE_TABLE;
  pa = LOCAL_PERIPH_BASE;
  while (pa < LOCAL_PERIPH_END) {
    pmd2[((pa - LOCAL_PERIPH_BASE) >> 21)] = pa | BOOT_DEVICE_ATTR;
    pa += PAGE_SIZE;
  }

  // --- setup_uvm (TTBR0) ---
  pgd0[0] = pud0_pa | TD_TYPE_TABLE;
  pud0[0] = pmd0_pa | TD_TYPE_TABLE;
  pmd0[0] = 0x0 | BOOT_NORMAL_ATTR;

  __asm__ volatile("dsb sy; isb");
}

static void free_region(uint64_t v, uint64_t e) {
  for (uint64_t start = PA_UP(v); start + PAGE_SIZE <= e; start += PAGE_SIZE) {
    if (start + PAGE_SIZE <= MEMORY_END) {
      kfree(start);
    }
  }
}

// 分配記憶體
void* buddy_alloc(int order) {
  for (int i = order; i < MAX_ORDER; i++) {
    if (!list_empty(&allocator.free_lists[i])) {
      // 找到空閒塊
      list_node_t* block = allocator.free_lists[i].next;
      list_del(block);

      // 如果找到的塊比需求大，則不斷分裂 (Split)
      while (i > order) {
        i--;
        // 計算分裂出的另一半（夥伴）
        uintptr_t buddy = (uintptr_t)block + (1UL << (i + PAGE_SHIFT));
        list_add(&allocator.free_lists[i], (list_node_t*)buddy);
      }
      return (void*)block;
    }
  }
  return NULL;  // 記憶體不足
}

// 釋放記憶體並嘗試合併 (Coalesce)
void buddy_free(void* ptr, int order) {
  uintptr_t addr = (uintptr_t)ptr;

  for (int i = order; i < MAX_ORDER - 1; i++) {
    // 計算夥伴地址
    uintptr_t buddy = allocator.start_addr + ((addr - allocator.start_addr) ^
                                              (1UL << (i + PAGE_SHIFT)));

    // 檢查夥伴是否在空閒串列中 (此處簡化檢查邏輯)
    bool buddy_found = false;
    list_node_t* curr;
    for (curr = allocator.free_lists[i].next; curr != &allocator.free_lists[i];
         curr = curr->next) {
      if ((uintptr_t)curr == buddy) {
        buddy_found = true;
        break;
      }
    }

    if (buddy_found) {
      // 合併：從目前層級移除夥伴，並往上一層遞迴
      list_del((list_node_t*)buddy);
      addr = addr < buddy ? addr : buddy;  // 取兩者較小者作為合併後的起始地址
    } else {
      break;  // 夥伴不在空閒串列，停止合併
    }
    list_add(&allocator.free_lists[i], (list_node_t*)addr);
  }

  // list_add(&allocator.free_lists[order], (list_node_t*)addr);
}

void kfree(uint64_t v) {
  ASSERT(v % PAGE_SIZE == 0);
  ASSERT(v >= (uint64_t)&end);
  ASSERT(v + PAGE_SIZE <= MEMORY_END);

#if MEMORYMANAGER == BuddyMode
  spin_lock(&allocator.lock);
  // 釋放 Order 0
  buddy_free((void*)v, 0);
  spin_unlock(&allocator.lock);
#else
  spin_lock(&free_memory.lock);
  struct Page* page_address = (struct Page*)v;

  page_address->next = free_memory.next;
  free_memory.next = page_address;
  spin_unlock(&free_memory.lock);
#endif
}
void* kalloc(void) {
  void* res = NULL;
#if MEMORYMANAGER == BuddyMode
  spin_lock(&allocator.lock);  // 記得給 Buddy Allocator 加鎖
  res = buddy_alloc(0);
  spin_unlock(&allocator.lock);
#else
  spin_lock(&free_memory.lock);

  // 強制從記憶體讀取指標，不使用暫存器快取
  struct Page* p = (struct Page*)*((volatile uint64_t*)&free_memory.next);

  if (p != NULL) {
    // 先將下一頁位址存好，再更新 head
    // 這裡同樣建議對 p->next 使用 volatile 讀取
    free_memory.next = p->next;
    res = (void*)p;
  }

  spin_unlock(&free_memory.lock);
#endif
  if (res) {
    memset(res, 0, PAGE_SIZE);
  }
  return res;
}
#if 0
void* kalloc(void) {
  spin_lock(&free_memory.lock);
  struct Page* page_address = free_memory.next;

  if (page_address != NULL) {
    ASSERT((uint64_t)page_address % PAGE_SIZE == 0);
    ASSERT((uint64_t)page_address >= (uint64_t)&end);
    ASSERT((uint64_t)page_address + PAGE_SIZE <= MEMORY_END);
    __asm__ volatile("dsb ish");
    free_memory.next = page_address->next;
  }
  spin_unlock(&free_memory.lock);

  return page_address;
}
#endif

void checkmm(void) {
  struct Page* v = free_memory.next;
  uint64_t size = 0;
  uint64_t i = 0;

  while (v != NULL) {
    size += PAGE_SIZE;
    printk("%d base is %x \r\n", i++, v);
    v = v->next;
  }

  printk("memory size is %u \r\n", size / 1024 / 1024);
}

static uint64_t* find_pgd_entry(uint64_t map, uint64_t v, int alloc,
                                uint64_t attribute) {
  uint64_t* ptr = (uint64_t*)map;
  void* addr = NULL;
  unsigned int index = (v >> 39) & 0x1ff;

  if (ptr[index] & ENTRY_V) {
    addr = (void*)P2V(PDE_ADDR(ptr[index]));
  } else if (alloc == 1) {
    addr = kalloc();
    if (addr != NULL) {
      memset(addr, 0, PAGE_SIZE);
      ptr[index] = (V2P(addr) | attribute | TABLE_ENTRY);
    }
  }

  return addr;
}

static uint64_t* find_pud_entry(uint64_t map, uint64_t v, int alloc,
                                uint64_t attribute) {
  uint64_t* ptr = NULL;
  void* addr = NULL;
  unsigned int index = (v >> 30) & 0x1ff;

  ptr = find_pgd_entry(map, v, alloc, attribute);
  if (ptr == NULL) {
    return NULL;
  }

  if (ptr[index] & ENTRY_V) {
    addr = (void*)P2V(PDE_ADDR(ptr[index]));
  } else if (alloc == 1) {
    addr = kalloc();
    if (addr != NULL) {
      memset(addr, 0, PAGE_SIZE);
      ptr[index] = (V2P(addr) | attribute | TABLE_ENTRY);
    }
  }

  return addr;
}
static uint64_t* find_pmd_entry(uint64_t map, uint64_t v, int alloc,
                                uint64_t attribute) {
  uint64_t* ptr = find_pud_entry(map, v, alloc, attribute);
  if (ptr == NULL) return NULL;

  unsigned int index = (v >> 21) & 0x1ff;
  void* addr = NULL;

  if (ptr[index] & ENTRY_V) {
    addr = (void*)P2V(PDE_ADDR(ptr[index]));
  } else if (alloc == 1) {
    addr = kalloc();
    if (addr != NULL) {
      memset(addr, 0, PAGE_SIZE);
      // 這裡必須是 TABLE_ENTRY，指向下一層 PTE
      ptr[index] = (V2P(addr) | attribute | TABLE_ENTRY);
    }
  }
  return (uint64_t*)addr;
}
bool map_page(uint64_t map, uint64_t v, uint64_t pa, uint64_t attribute) {
  uint64_t vstart = PA_DOWN(v);
  uint64_t* ptr = NULL;

  ASSERT(vstart + PAGE_SIZE < MEMORY_END);
  ASSERT(pa % PAGE_SIZE == 0);
  ASSERT(pa + PAGE_SIZE <= V2P(MEMORY_END));

  ptr = find_pud_entry(map, vstart, 1, attribute);
  if (ptr == NULL) {
    return false;
  }

  unsigned int index = (vstart >> 21) & 0x1ff;
  ASSERT((ptr[index] & ENTRY_V) == 0);

  ptr[index] = (pa | attribute | BLOCK_ENTRY);

  return true;
}

void free_page(uint64_t map, uint64_t vstart) {
#if 1
  uint64_t* ptr = find_pud_entry(map, vstart, 0, 0);

  if (ptr != NULL) {
    unsigned int index = (vstart >> 21) & 0x1ff;
    if (ptr[index] & ENTRY_V) {
      // 這裡釋放的是當初 map_page 映射進去的實體頁面 (2MB)
      kfree((uint64_t)P2V(PTE_ADDR(ptr[index])));
      ptr[index] = 0;
    }
  }
#else
  unsigned int index;
  uint64_t* ptr = NULL;

  ASSERT(vstart % PAGE_SIZE == 0);

  ptr = find_pud_entry(map, vstart, 0, 0);

  if (ptr != NULL) {
    index = (vstart >> 21) & 0x1ff;

    if (ptr[index] & ENTRY_V) {
      kfree(P2V(PTE_ADDR(ptr[index])));
      ptr[index] = 0;
    }
  }
#endif
}

static void free_pmd(uint64_t map) {
  uint64_t* pgd = (uint64_t*)map;
  uint64_t* pud = NULL;

  for (int i = 0; i < 512; i++) {
    if (pgd[i] & ENTRY_V) {
      pud = (uint64_t*)P2V(PDE_ADDR(pgd[i]));

      for (int j = 0; j < 512; j++) {
        if (pud[j] & ENTRY_V) {
          kfree(P2V(PDE_ADDR(pud[j])));
          pud[j] = 0;
        }
      }
    }
  }
}

static void free_pud(uint64_t map) {
  uint64_t* ptr = (uint64_t*)map;

  for (int i = 0; i < 512; i++) {
    if (ptr[i] & ENTRY_V) {
      kfree(P2V(PDE_ADDR(ptr[i])));
      ptr[i] = 0;
    }
  }
}

static void free_pgd(uint64_t map) { kfree(map); }

void free_table(uint64_t* table, int level) {
  for (int i = 0; i < 512; i++) {
    if (table[i] & ENTRY_V) {
      uint64_t child_pa = PDE_ADDR(table[i]);
      // 如果不是最後一層 (Block/Page)，則繼續往下走
      // 在你的案例中，PUD 指向的是下一層 Table
      if (level < 2) {
        free_table((uint64_t*)P2V(child_pa), level + 1);
      }
      // 釋放這一層頁表目錄本身
      kfree((uint64_t)P2V(child_pa));
    }
  }
}
void free_vm(uint64_t map) {
#if 1

  free_page(map, 0x400000);

  uint64_t* pgd = (uint64_t*)map;
  free_table(pgd, 0);
  kfree(map);  // 最後釋放 PGD 本身
#else
  free_page(map, 0x400000);
  free_pmd(map);
  free_pud(map);
  free_pgd(map);
#endif
}

bool setup_uvm(struct Process* process, char* file_name) {
  bool status = false;
  uint64_t map = process->page_map;
  void* page = kalloc();

  if (page != NULL) {
    memset(page, 0, PAGE_SIZE);
    status = map_page(map, 0x400000, V2P(page),
                      ENTRY_V | USER | NORMAL_MEMORY | ENTRY_ACCESSED);

    if (status == true) {
      int fd = open_file(process, file_name);
      if (fd == -1) {
        free_vm(map);
        return false;
      }

      uint32_t size = get_file_size(process, fd);

      if (read_file(process, fd, page, size) != size) {
        free_vm(map);
        return false;
      }

      close_file(process, fd);
    } else {
      kfree((uint64_t)page);
      free_vm(map);
    }
  }

  return status;
}

bool copy_uvm(uint64_t dst_map, uint64_t src_map, int size) {
#if 1
  void* new_page = kalloc();  // 為新進程分配一個 2MB 塊
  if (!new_page) return false;

  memset(new_page, 0, PAGE_SIZE);

  // 尋找原進程的頁面位址
  uint64_t* pd = find_pud_entry(src_map, 0x400000, 0, 0);
  unsigned int index = (0x400000U >> 21) & 0x1FF;
  uint64_t src_pa = P2V(PTE_ADDR(pd[index]));

  // 複製資料
  memcpy(new_page, (void*)src_pa, size);

  // 建立新進程的映射
  return map_page(dst_map, 0x400000, V2P(new_page),
                  ENTRY_V | USER | NORMAL_MEMORY | ENTRY_ACCESSED);
#else
  bool status = false;
  unsigned int index;
  uint64_t* pd = NULL;
  uint64_t start;

  void* page = kalloc();

  if (page != NULL) {
    memset(page, 0, PAGE_SIZE);
    status = map_page(dst_map, 0x400000, V2P(page),
                      ENTRY_V | USER | NORMAL_MEMORY | ENTRY_ACCESSED);

    if (status == true) {
      pd = find_pud_entry(src_map, 0x400000, 0, 0);
      if (pd == NULL) {
        free_vm(dst_map);
        return false;
      }

      index = (0x400000U >> 21) & 0x1FF;
      ASSERT((pd[index] & ENTRY_V) == 1);
      start = P2V(PTE_ADDR(pd[index]));
      memcpy(page, (void*)start, size);
    } else {
      kfree((uint64_t)page);
      free_vm(dst_map);
    }
  }

  return status;
#endif
}

void switch_vm(uint64_t map) { load_pgd(V2P(map)); }

extern char end;  // 連結器定義的核心結束位址
#if 0
void check_page_table_safety(void) {
  uint64_t kernel_end = (uint64_t)&end;

  printk("\n========== Memory Layout Safety Check ==========\n");
  printk("Kernel End Symbol (&end): 0x%x\n", kernel_end);

  // 檢查 TTBR1 (Kernel Space Tables)
  printk("\n[TTBR1 - Kernel Maps]\n");
  printk("  pgd_ttbr1: 0x%x %s\n", (uint64_t)&pgd_ttbr1,
         ((uint64_t)&pgd_ttbr1 >= kernel_end) ? "[DANGER]" : "[SAFE]");
  printk("  pud_ttbr1: 0x%x %s\n", (uint64_t)&pud_ttbr1,
         ((uint64_t)&pud_ttbr1 >= kernel_end) ? "[DANGER]" : "[SAFE]");
  printk("  pmd_ttbr1: 0x%x %s\n", (uint64_t)&pmd_ttbr1,
         ((uint64_t)&pmd_ttbr1 >= kernel_end) ? "[DANGER]" : "[SAFE]");

  // 檢查 TTBR0 (User Space Initial Tables)
  printk("\n[TTBR0 - User Maps]\n");
  printk("  pgd_ttbr0: 0x%x %s\n", (uint64_t)&pgd_ttbr0,
         ((uint64_t)&pgd_ttbr0 >= kernel_end) ? "[DANGER]" : "[SAFE]");
  printk("  pud_ttbr0: 0x%x %s\n", (uint64_t)&pud_ttbr0,
         ((uint64_t)&pud_ttbr0 >= kernel_end) ? "[DANGER]" : "[SAFE]");
  printk("  pmd_ttbr0: 0x%x %s\n", (uint64_t)&pmd_ttbr0,
         ((uint64_t)&pmd_ttbr0 >= kernel_end) ? "[DANGER]" : "[SAFE]");

  // 檢查潛在衝突
  if (kernel_end < (uint64_t)&pmd_ttbr0 || kernel_end < (uint64_t)&pmd2_ttbr1) {
    printk("\n!!! WARNING !!!\n");
    printk("Some page tables are located AFTER &end.\n");
    printk("init_memory() will treat them as FREE MEMORY!\n");
    printk("This causes Core 1 to see corrupted addresses.\n");
  } else {
    printk("\n[RESULT] All static page tables are protected.\n");
  }
  printk("==============================================\n\n");
}
#endif
void init_memory(void) {
// check_page_table_safety();
#if MEMORYMANAGER == BuddyMode
  printk("Buddy Allocator initialized from 0x%x to 0x%x\n",
         PA_UP((uint64_t)&end), MEMORY_END);
  buddy_init(PA_UP((uint64_t)&end), MEMORY_END - PA_UP((uint64_t)&end));

#else
  free_region((uint64_t)&end, MEMORY_END);
#endif

  // checkmm();
}