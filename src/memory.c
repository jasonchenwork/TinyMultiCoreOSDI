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
void load_pgd(uint64_t map);
 /* TTBR1 - 核心空間頁表 (Kernel Space) */
extern uint64_t pgd_ttbr1[512] __attribute__((aligned(4096)));;
extern uint64_t pud_ttbr1[512] __attribute__((aligned(4096)));;
extern uint64_t pmd_ttbr1[512] __attribute__((aligned(4096)));;
extern uint64_t pmd2_ttbr1[512] __attribute__((aligned(4096)));;
 

/* TTBR0 - 用戶空間頁表 (User Space) */
extern uint64_t pgd_ttbr0[512] __attribute__((aligned(4096)));;
extern uint64_t pud_ttbr0[512] __attribute__((aligned(4096)));;
extern uint64_t pmd_ttbr0[512] __attribute__((aligned(4096)));;
 


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

void kfree(uint64_t v) {
  ASSERT(v % PAGE_SIZE == 0);
  ASSERT(v >= (uint64_t)&end);
  ASSERT(v + PAGE_SIZE <= MEMORY_END);
  spin_lock(&free_memory.lock);
  struct Page* page_address = (struct Page*)v;

  page_address->next = free_memory.next;
  free_memory.next = page_address;

  spin_unlock(&free_memory.lock);
}
void* kalloc(void) {
  void* res = NULL;
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

void free_vm(uint64_t map) {
  free_page(map, 0x400000);
  free_pmd(map);
  free_pud(map);
  free_pgd(map);
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
  free_region((uint64_t)&end, MEMORY_END);
  // checkmm();
}