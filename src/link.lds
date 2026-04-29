ENTRY(start)
SECTIONS
{
    /*  . = 0x000000080000; */ 
    . = 0xffff000000080000;

    .text :{
        KEEP(*(.text.boot))
        *(.text)
         . = ALIGN(16);
    }
    
    .rodata : {
        *(.rodata)
        . = ALIGN(16);
    }
    
    .data : {
        *(.data)
        . = ALIGN(16);
    }
    .bss : {
        bss_start = .;
        *(.bss)
        bss_end = .;
        . = ALIGN(16);
    }

    . += 0x4000;      /* 預留 16KB 給 4 個核心 (每個核 4KB) */
    stack_top = .;    /* 將當前位址賦值給 stack_top */
    end = .;

}