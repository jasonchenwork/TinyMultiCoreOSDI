.equ MAIR_ATTR, (0xFF | (0xFF << 8)) // Attr 0 & 1 都是 WBWA Cacheable
.equ TCR_T0SZ,  (16) 
.equ TCR_T1SZ,  (16 << 16)
.equ TCR_TG0,   (0 << 14)
.equ TCR_TG1,   (2 << 30)
.equ TCR_SH0,     (3 << 12)          // Inner Shareable (多核心同步必備)
.equ TCR_ORGN0,   (1 << 10)          // Normal memory, Outer Write-Back Read-Allocate Write-Allocate Cacheable
.equ TCR_IRGN0,   (1 << 8)           // Normal memory, Inner Write-Back Read-Allocate Write-Allocate Cacheable
.equ TCR_VALUE,   (TCR_T0SZ | TCR_T1SZ | TCR_TG0 | TCR_TG1 | TCR_SH0 | TCR_ORGN0 | TCR_IRGN0)
// 1<<10 (AF) | 3<<8 (SH: Inner) | 1<<2 (AttrIndx 1) | 1<<0 (Valid)
.equ ATTR_NORMAL_INNER, (1 << 10 | 3 << 8 | 1 << 2 | 1 << 0)

.equ PAGE_SIZE, (2*1024*1024)

.equ SCTLR_MMU_ENABLE, (1<<0)
.equ SCTLR_ALIGN_CHECK, (1<<1)
.equ SCTLR_D_CACHE, (1<<2)
.equ SCTLR_I_CACHE, (1<<12)

.global enable_mmu
.global setup_vm
.global load_pgd
.global read_pgd





read_pgd:
    mrs x0, ttbr0_el1
    ret

load_pgd:
    msr ttbr0_el1, x0
    tlbi vmalle1is
    dsb ish
    isb

    ret
 
enable_mmu:
    adr x0, pgd_ttbr1
    msr ttbr1_el1, x0

    adr x0, pgd_ttbr0
    msr ttbr0_el1, x0

    ldr x0, =MAIR_ATTR
    msr mair_el1, x0

    ldr x0, =TCR_VALUE
    msr tcr_el1, x0

    mrs x0, sctlr_el1
    mov x1, #(SCTLR_MMU_ENABLE | SCTLR_D_CACHE | SCTLR_I_CACHE)
    orr x0, x0, x1
    msr sctlr_el1, x0
    // [重要] 確保 TCR/MAIR 寫入完成，且之前的記憶體寫入對所有核心可見
    dsb sy
    isb
    ret
 

setup_vm:
setup_kvm:
    adr x0, pgd_ttbr1
    adr x1, pud_ttbr1
    orr x1, x1, #3
    str x1, [x0]              // pgd[0] = pud

    mov x2, #511 * 8
    add x0, x0, x2
    str x1, [x0]              // pgd[511] = pud

    adr x0, pud_ttbr1
    adr x1, pmd_ttbr1
    orr x1, x1, #3
    str x1, [x0]              // pud[0] = pmd

    mov x2, #5 * 8
    add x0, x0, x2
    str x1, [x0]              // pud[5] = pmd              // pud[0] = pmd

    mov x2, #5 * 8
    add x0, x0, x2
    str x1, [x0]              // pud[5] = pmd

    mov x2, #0x34000000
    adr x1, pmd_ttbr1
    //mov x0, #(1 << 10 | 1 << 2 | 1 << 0)
    mov x0, #ATTR_NORMAL_INNER

loop1:
    str x0, [x1], #8
    add x0, x0, #PAGE_SIZE
    cmp x0, x2
    blo loop1

    // Set pmd[0x4] for kernel block
    adr x1, pmd_ttbr1
    add x1, x1, #(0x4 * 8)
    mov x0, #0x80000
    orr x0, x0, #1
    orr x0, x0, #(1 << 10)
    orr x0, x0, #(1 << 2)
    orr x0, x0, #(3 << 8)
    str x0, [x1]

    mov x2, #0x40200000
    mov x0, #0x3f000000

    adr x3, pmd_ttbr1
    lsr x1, x0, #(21 - 3)
    add x1, x1, x3

    orr x0, x0, #1
    orr x0, x0, #(1 << 10)
    orr x0, x0, #(3 << 8)

loop2:
    str x0, [x1], #8
    add x0, x0, #PAGE_SIZE
    cmp x0, x2
    blo loop2

    adr x0, pud_ttbr1
    add x0, x0, #(1 * 8)
    adr x1, pmd2_ttbr1
    orr x1, x1, #3
    str x1, [x0]

    mov x2, #0x41000000
    mov x0, #0x40000000

    adr x1, pmd2_ttbr1
    orr x0, x0, #1
    orr x0, x0, #(1 << 10)
    orr x0, x0, #(3 << 8)

loop3:
    str x0, [x1], #8
    add x0, x0, #PAGE_SIZE
    cmp x0, x2
    blo loop3


setup_uvm:
    adr x0, pgd_ttbr0
    adr x1, pud_ttbr0
    orr x1, x1, #3
    str x1, [x0]

    adr x0, pud_ttbr0
    adr x1, pmd_ttbr0
    orr x1, x1, #3
    str x1, [x0]

    adr x1, pmd_ttbr0
    //mov x0, #(1 << 10 | 1 << 2 | 1 << 0)
    mov x0, #ATTR_NORMAL_INNER
    str x0, [x1]

    ret

.global pgd_ttbr1
.global pud_ttbr1
.global pmd_ttbr1
.global pmd2_ttbr1
.global pgd_ttbr0
.global pud_ttbr0
.global pmd_ttbr0

.balign 4096
pgd_ttbr1:
    .space 4096
pud_ttbr1:
    .space 4096
pmd_ttbr1:
    .space 4096
pmd2_ttbr1:
    .space 4096

pgd_ttbr0:
    .space 4096
pud_ttbr0:
    .space 4096
pmd_ttbr0:
    .space 4096

 