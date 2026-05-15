# Tiny Multi-core Enhanced Operating System

這是一個基於單核心小型作業系統實作並進一步擴展的多核心作業系統專案。本專案的核心目標是在原有的單核架構基礎上，引入 多核心支援，並實現基礎的多核心排程與進程管理功能
**為了要加入多核心處裡需要實作spin lock/ spin unlock**
```asm
 spin_lock:

    mov     w1, #1
1:
    ldaxr   w2, [x0]        // 讀取並鎖定 (Acquire)
    cbnz    w2, 2f          // 如果值不為 0，跳到 2f 等待
    stxr    w3, w1, [x0]    // 嘗試寫入 1 (Exclusive)
    cbnz    w3, 1b          // 如果寫入失敗 (w3=1)，回到 1b 重新嘗試
    ret
2:
    yield                   // 提示 CPU 正在自旋，降低功耗
    b       1b              // 重新檢查

// x0: 鎖的位址
spin_unlock:
    stlr    wzr, [x0]       // 將 0 存入並釋放 (Release)

    ret
```

**如何喚醒其他核心**  

```C
  volatile uint64_t* spin_table =
      (volatile uint64_t*)(uintptr_t)(0xD8 + (core_id) * 8);

  *spin_table = (0x80000);  // 寫入入口點
  __asm__ volatile("dmb sy");  // Data Memory Barrier
  __asm__ volatile("sev"); //Send Event
```
**新增CMD PS(process state)**  

![](https://github.com/jasonchenwork/TinyMultiCoreOSDI/blob/main/image/ps.bmp)

**新增 buddy system**  
```C
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
```

### Build
```bash
make
```

### Run on QEMU
```bash
make run 
```

### Clean 
```bash
make clean
```

## Directory structure
```
.
├── README.md
├── Makefile    
├── data
│   └── os.img
├── include                 
│   ├── debug.h          
│   ├── file.h
│   ├── handler.h
│   ├── irq.h
│   ├── lib.h
│   ├── memory.h
│   ├── print.h              
│   ├── process.h            
│   └── uart.h
└── src                     
    ├── boot.s          
    ├── handler.s
    ├── liba.s
    ├── mmu.s
    ├── debug.c
    ├── file.c
    ├── handler.c
    ├── keyboard.c               
    ├── lib.c
    ├── main.c            
    ├── memory.c
    ├── print.c
    ├── process.c
    ├── syscall.c
    ├── uart.c
    └── link.lds
```  
