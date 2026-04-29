.global delay
.global out_word
.global in_word
.global memset
.global memcpy
.global memmove
.global memcmp
.global get_el
.global getcpuid

.global spin_lock
.global spin_unlock

getcpuid:
    mrs x0, mpidr_el1
    and x0, x0, #3
    ret
    
get_el:
    mrs x0, currentel
    lsr x0, x0, #2
    ret

delay:
    subs x0, x0, #1
    bne delay
    ret

out_word:
    str w1, [x0]
    ret

in_word:
    ldr w0, [x0]
    ret

memset:
    cmp x2, #0
    beq memset_end

set:
    strb w1, [x0], #1
    subs x2, x2, #1
    bne set

memset_end:
    ret

memcmp:
    mov x3, x0
    mov x0, #0

compare:
    cmp x2, #0
    beq memcmp_end

    ldrb w4, [x3], #1
    ldrb w5, [x1], #1
    sub x2, x2, #1
    cmp w4, w5
    beq compare

    mov x0, #1

memcmp_end:
    ret

memmove:
memcpy:
    cmp x2, #0
    beq memcpy_end

    mov x4, #1

    cmp x1, x0
    bhs copy
    add x3, x1, x2
    cmp x3, x0
    bls copy

overlap:
    sub x3, x2, #1
    add x0, x0, x3
    add x1, x1, x3
    neg x4, x4

copy:
    ldrb w3, [x1]
    strb w3, [x0]
    add x0, x0, x4
    add x1, x1, x4

    subs x2, x2, #1
    bne copy

memcpy_end:
    ret



// x0: 鎖的位址 (pointer to int)
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
