# Tiny Multi-core Enhanced Operating System

這是一個基於開源小型作業系統實作並進一步擴展的多核心作業系統專案。本專案的核心目標是在原有的單核架構基礎上，引入 多核心支援，並實現基礎的多核心排程與進程管理功能
**為了要加入多核心處裡需要實作spin lock/ spin unlock**
```asm
 spin_lock:
    //msr     daifset, #2     // 2. 關閉本地 IRQ
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
    //msr     daifclr, #2     // 正確的指令是 daifclr (開啟 IRQ)
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