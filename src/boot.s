.equ FS_BASE, 0xffff000030000000
.equ FS_SIZE, 101*16*63*512

.section ".text.boot"
.global start
.global el1_entry
.extern setup_vm
.extern setup_vm2
.extern enable_mmu

start:
    mrs x0, currentel
    lsr x0, x0, #2
    cmp x0, #2
    beq from_el2
    cmp x0, #1
    beq from_el1
    b end

from_el2:
    b kernel_entry

from_el1:
    b el1_entry

end:
    b end
invalidate_dcache:
    mrs     x0, clidr_el1
    and     w3, w0, #0x07000000    // 取得 LoC (Level of Coherency)
    lsr     w3, w3, #23
    cbz     w3, finished

    mov     w10, #0                // w10 = 目前層級 (Level, 32-bit)
loop_level:
    add     w2, w10, w10, lsl #1   // w2 = level * 3
    lsr     x1, x0, x2             // 修正：全部使用 64-bit 暫存器進行位移
    and     w1, w1, #0x7           // 取得該層級的 Cache 類型
    cmp     w1, #2
    b.lt    next_level             

    msr     csselr_el1, x10        // 選擇該層級
    isb
    mrs     x1, ccsidr_el1         // 讀取該層級資訊 (CCSIDR_EL1 是 64-bit)
    
    and     w2, w1, #0x7           // Line Size (32-bit ok)
    add     w2, w2, #4             // Log2(Line Size)
    
    mov     w4, #0x3ff
    lsr     x12, x1, #3            // 暫存位移結果
    and     w4, w4, w12            // Max Way number
    clz     w5, w4                 // Way shift
    
    mov     w7, #0x7fff
    lsr     x12, x1, #13           // 暫存位移結果
    and     w7, w7, w12            // Max Set number

loop_set:
    mov     w8, w4                 // 目前 Way
loop_way:
    // 透過寄存器位移合成 ISW 操作數
    lsl     w9, w8, w5             // Way shift
    orr     w9, w9, w10            // 組合 Level
    lsl     w11, w7, w2            // Set shift
    orr     w9, w9, w11            // 組合 Set
    
    dc      isw, x9                // Data Cache Invalidate by Set/Way
    subs    w8, w8, #1
    b.ge    loop_way
    
    subs    w7, w7, #1
    b.ge    loop_set

next_level:
    add     w10, w10, #2
    cmp     w3, w10
    b.gt    loop_level

finished:
    dsb     sy
    isb
    ret

kernel_entry:
    mrs x0, currentel
    lsr x0, x0, #2
    cmp x0, #2
    bne end

    msr sctlr_el1, xzr
    mov x0, #(1 << 31)
    msr hcr_el2, x0

    mov x0, #0b1111000101
    msr spsr_el2, x0

    adr x0, el1_entry
    msr elr_el2, x0

    eret

el1_entry:
    #mov sp, #0x80000
    mrs x0, mpidr_el1
    and x0, x0, #0xFF   // 取得 CPU ID
    
    ldr x1, =stack_top
    //mov x3, #0xffff000000000000
    //sub x1, x1, x3

    mov x2, #4096       // 每個核 4KB
    mul x2, x0, x2
    sub x1, x1, x2      // 計算該核的棧頂
    mov sp, x1          // 正確設定 SP_EL1

    cmp x0, #0          // 通常建議讓 Core 0 當 Master
    beq master_init

slave_loop:
   
    //bl setup_vm
   // bl invalidate_dcache
    bl enable_mmu        // 假設你的 enable_mmu 是多核安全的
    
    //mov x0, #0xffff000000000000
    //add sp, sp, x0       // 切換到虛擬位址 SP
    
    //wfe                  // 等待事件信號（由 core0 的 sev 喚醒）
    ldr x0, =vector_table
    msr vbar_el1, x0
    
    ldr x0, =secondary_main
    blr x0
    
    b   halt             // 其他核先休息

master_init:
    //bl invalidate_dcache

    mov sp, #0x80000
    bl setup_vm2
    bl enable_mmu

    ldr x0, =FS_BASE
    ldr x1, =bss_start
    ldr x2, =FS_SIZE
    bl memcpy

    ldr x0, =bss_start
    ldr x1, =bss_end
    sub x2, x1, x0
    mov x1, #0
    bl memset

    ldr x0, =vector_table
    msr vbar_el1, x0
    
 
    ldr x1, =stack_top
    mov sp, x1

    ldr x0, =KMain
    blr x0

halt:
    wfi
    b halt
    