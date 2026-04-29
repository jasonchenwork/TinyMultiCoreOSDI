#include "debug.h"
#include "file.h"
// #include "cpio.h"
#include "handler.h"
#include "lib.h"
#include "memory.h"
#include "print.h"
#include "process.h"
#include "stddef.h"
#include "syscall.h"
#include "uart.h"

extern void el1_entry(void);
volatile int main_core_ready = 0;
#include "print.h"
#include "stdint.h"

// 定義 SCTLR_EL1 的位元遮罩
#define SCTLR_M (1 << 0)    // MMU enable
#define SCTLR_A (1 << 1)    // Alignment check enable
#define SCTLR_C (1 << 2)    // Data cache enable
#define SCTLR_I (1 << 12)   // Instruction cache enable
#define SCTLR_EE (1 << 25)  // Endianness (0: Little, 1: Big)

void check_sctlr_el1(void) {
  uint64_t sctlr;
  int core = getcpuid();

  // 使用內聯組合語言讀取暫存器
  __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));

  printk("--- Core %d SCTLR_EL1: 0x%x ---\n", core, sctlr);

  printk("  MMU: %s\n", (sctlr & SCTLR_M) ? "Enabled" : "Disabled");
  printk("  Data Cache: %s\n", (sctlr & SCTLR_C) ? "Enabled" : "Disabled");
  printk("  Instruction Cache: %s\n",
         (sctlr & SCTLR_I) ? "Enabled" : "Disabled");
  printk("  Alignment Check: %s\n", (sctlr & SCTLR_A) ? "Enabled" : "Disabled");
  printk("  Endianness: %s\n",
         (sctlr & SCTLR_EE) ? "Big-endian" : "Little-endian");

  // 特別檢查多核最關鍵的兩個位元
  if (!(sctlr & SCTLR_C)) {
    printk("  [WARNING] Data Cache is OFF on Core %d!\n", core);
  }

  if (!(sctlr & SCTLR_M)) {
    printk("  [WARNING] MMU is OFF on Core %d! Physical addresses used.\n",
           core);
  }
  printk("------------------------------\n");
}
void secondary_main() {
  // 這裡由 Core 1 執行
  // 注意：printk 必須是多核安全的（內含 Spinlock），否則會跟 Core 0 互卡
  // core1_started = 1;
  uint64_t core_id = getcpuid();
#if 0
  extern char pmd_ttbr0;
  printk("[Core %u] Last static table end at: %x\n", core_id,
         &pmd_ttbr0 + 4096);
#endif
  while (main_core_ready == 0) {
    __asm__ volatile("nop");
  }

  // check_sctlr_el1();

  init_process();

  printk("Core %u started\r\n", core_id);
  init_timer();
  enable_irq();
  while (1) {
  }
}
void start_secondary_core(int core_id) {
  if (core_id < 1 || core_id > 3) return;
  // 試試看 QEMU 低位址 Spin-table (這是 64-bit address)
  volatile uint64_t* spin_table =
      (volatile uint64_t*)(uintptr_t)(0xD8 + (core_id) * 8);

  *spin_table = (0x80000);  // 寫入入口點
  __asm__ volatile("dmb sy");
  __asm__ volatile("sev");

  // printk("Spin-table 0x%x value: %x\r\n", (uint32_t)spin_table, *spin_table);
}
void dump_os_img_head(char* msg) {
  extern unsigned char end;  // 取得 Linker Script 中的 end 符號
  unsigned char* ptr = &end;

  printk("--- Dump at %s (Addr: %x) ---\n", msg, ptr);
  for (int i = 0; i < 32; i++) {
    printk("%x ", ptr[i]);
    if ((i + 1) % 16 == 0) printk("\n");
  }
  printk("----------------------------\n");
}
void KMain(void) {
  init_uart();

  printk("Hello, Raspberry pi\r\n");
  // printk("We are at EL %u\r\n", (uint64_t)get_el());
  printk("Core 0 started successfully!\n");
  // check_sctlr_el1();
  start_secondary_core(1);
  start_secondary_core(2);
  start_secondary_core(3);
#if 0
  printk("Core 0 started successfully!\n");

  while (1) {
  }
#endif

  init_memory();
  init_fs();
  init_system_call();

  init_process();
  init_timer();

  init_interrupt_controller();
  main_core_ready = 1;
  enable_irq();
#if 0
  while (1) {
    printk("Core 0 is running Core 0:%d Core 1:%d\n", core_ticks[0],
           core_ticks[1]);
    for (int i = 0; i < 100000000; i++);
  }
#endif
}