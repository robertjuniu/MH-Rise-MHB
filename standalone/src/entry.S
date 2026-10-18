// SPDX-License-Identifier: GPL-2.0-or-later
.section .text.entry,"ax"
.global __module_start
__module_start:
    b 1f
    .word __mod0 - __module_start
1:  ret
    // Eden NCE scans instructions from offset 0x24; reserve the module header.
    .space 0x40 - (. - __module_start), 0

.section .rodata.mod0,"a"
.balign 4
__mod0:
    .ascii "MOD0"
    .word __dynamic_start - __mod0
    .word __bss_start - __mod0
    .word __bss_end - __mod0
    .word 0
    .word 0
    .word mhr_module_runtime - __mod0

.section .text.svc,"ax"
.global mhr_debug_string
mhr_debug_string:
    svc #0x27
    ret

.global mhr_query_memory
mhr_query_memory:
    str x1, [sp, #-16]!
    svc #0x6
    ldr x2, [sp], #16
    str w1, [x2]
    ret

.global mhr_get_info
mhr_get_info:
    str x0, [sp, #-16]!
    svc #0x29
    ldr x2, [sp], #16
    str x1, [x2]
    ret

.global mhr_map_process
mhr_map_process:
    svc #0x74
    ret
.global mhr_unmap_process
mhr_unmap_process:
    svc #0x75
    ret
.global mhr_close_handle
mhr_close_handle:
    svc #0x16
    ret
.global mhr_create_thread
mhr_create_thread:
    str x0, [sp, #-16]!
    svc #0x8
    ldr x2, [sp], #16
    str w1, [x2]
    ret
.global mhr_start_thread
mhr_start_thread:
    svc #0x9
    ret
.global mhr_sleep
mhr_sleep:
    svc #0xb
    ret
.global mhr_get_tick
mhr_get_tick:
    svc #0x1e
    ret

.global sqrtf
sqrtf:
    fsqrt s0, s0
    ret
.global fabsf
fabsf:
    fabs s0, s0
    ret
.global ceilf
ceilf:
    frintp s0, s0
    ret
.global fmaxf
fmaxf:
    fmaxnm s0, s0, s1
    ret
.global fminf
fminf:
    fminnm s0, s0, s1
    ret

.global mhr_cache_sync
mhr_cache_sync:
    add x3, x1, x2
    and x1, x1, #-64
1:  dc cvau, x1
    add x1, x1, #64
    cmp x1, x3
    b.lo 1b
    dsb ish
    add x3, x0, x2
    and x0, x0, #-64
2:  ic ivau, x0
    add x0, x0, #64
    cmp x0, x3
    b.lo 2b
    dsb ish
    isb
    ret
