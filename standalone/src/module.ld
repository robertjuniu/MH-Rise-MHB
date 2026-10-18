OUTPUT_FORMAT(elf64-littleaarch64)
OUTPUT_ARCH(aarch64)
ENTRY(__module_start)
PHDRS {
    text PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
    dynamic PT_DYNAMIC FLAGS(6);
}
SECTIONS {
    . = 0;
    .text : { KEEP(*(.text.entry)) *(.text .text.*) } :text
    . = ALIGN(0x1000);
    .rodata : { KEEP(*(.rodata.mod0)) *(.rodata .rodata.*) } :rodata
    .hash : { *(.hash) } :rodata
    .gnu.hash : { *(.gnu.hash) } :rodata
    .dynsym : { *(.dynsym) } :rodata
    .dynstr : { *(.dynstr) } :rodata
    .rela.dyn : { *(.rela.dyn) } :rodata
    .rela.plt : { *(.rela.plt) } :rodata
    . = ALIGN(0x1000);
    .data : { *(.data .data.*) *(.got .got.*) } :data
    .dynamic : { __dynamic_start = .; *(.dynamic) } :data :dynamic
    .init_array : {
        __init_array_start = .;
        KEEP(*(.init_array .init_array.*))
        __init_array_end = .;
    } :data
    . = ALIGN(16);
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.*) *(COMMON)
        . = ALIGN(16);
        __bss_end = .;
    } :data
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
