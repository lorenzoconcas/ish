#include "gadgets.h"
#include "emu/interrupt.h"

.gadget push
    sub _addr, esp, 4
    write_prep 32, push
    str _tmp, [_xaddr]
    write_done 32, push
    sub esp, esp, 4
    gret 1
    write_bullshit 32, push
.gadget push64
    sub _xaddr, xsp, 8
    write_prep 64, push64
    str _xtmp, [_xaddr]
    write_done 64, push64
    sub xsp, xsp, 8
    gret 1
    write_bullshit 64, push64
.gadget pop
    mov _addr, esp
    read_prep 32, pop
    ldr _tmp, [_xaddr]
    add esp, esp, 4
    gret 1
    read_bullshit 32, pop
.gadget pop64
    mov _xaddr, xsp
    read_prep 64, pop64
    ldr _xtmp, [_xaddr]
    add xsp, xsp, 8
    gret 1
    read_bullshit 64, pop64

.macro x name, reg
    .gadget addr_\name
        ldr _addr, [_ip]
        add _addr, _addr, \reg
        gret 1
.endm
.each_reg x
.purgem x
.macro addr_high name, offset
    .gadget addr_\name
        ldr _addr, [_ip]
        ldr w8, [_cpu, \offset]
        add _addr, _addr, w8
        gret 1
.endm
addr_high reg_r8, CPU_r8
addr_high reg_r9, CPU_r9
addr_high reg_r10, CPU_r10
addr_high reg_r11, CPU_r11
addr_high reg_r12, CPU_r12
addr_high reg_r13, CPU_r13
addr_high reg_r14, CPU_r14
addr_high reg_r15, CPU_r15
.purgem addr_high
.gadget addr_none
    ldr _addr, [_ip]
    gret 1
.gadget_list addr, REG_LIST

.macro addr64_low name, reg
    .gadget addr64_\name
        ldr _xaddr, [_ip]
        add _xaddr, _xaddr, \reg
        gret 1
.endm
addr64_low reg_a, xax
addr64_low reg_c, xcx
addr64_low reg_d, xdx
addr64_low reg_b, xbx
addr64_low reg_sp, xsp
addr64_low reg_bp, xbp
addr64_low reg_si, xsi
addr64_low reg_di, xdi
.purgem addr64_low
.macro addr64_high name, offset
    .gadget addr64_\name
        ldr _xaddr, [_ip]
        ldr x8, [_cpu, \offset]
        add _xaddr, _xaddr, x8
        gret 1
.endm
addr64_high reg_r8, CPU_r8
addr64_high reg_r9, CPU_r9
addr64_high reg_r10, CPU_r10
addr64_high reg_r11, CPU_r11
addr64_high reg_r12, CPU_r12
addr64_high reg_r13, CPU_r13
addr64_high reg_r14, CPU_r14
addr64_high reg_r15, CPU_r15
.purgem addr64_high
.gadget addr64_none
    ldr _xaddr, [_ip]
    gret 1
.gadget_list addr64, REG_LIST

.macro x name, reg
    .irp times, 1,2,4,8
        .gadget si_\name\()_\times
            .ifc \times,1
                add _addr, _addr, \reg
            .else N .ifc \times,2
                add _addr, _addr, \reg, lsl 1
            .else N .ifc \times,4
                add _addr, _addr, \reg, lsl 2
            .else N .ifc \times,8
                add _addr, _addr, \reg, lsl 3
            .endif N .endif N .endif N .endif
            gret
    .endr
.endm
.each_reg x
.purgem x

.macro si_high name, offset
    .irp times, 1,2,4,8
        .gadget si_\name\()_\times
            ldr w8, [_cpu, \offset]
            .ifc \times,1
                add _addr, _addr, w8
            .else N .ifc \times,2
                add _addr, _addr, w8, lsl 1
            .else N .ifc \times,4
                add _addr, _addr, w8, lsl 2
            .else N .ifc \times,8
                add _addr, _addr, w8, lsl 3
            .endif N .endif N .endif N .endif
            gret
    .endr
.endm
si_high reg_r8, CPU_r8
si_high reg_r9, CPU_r9
si_high reg_r10, CPU_r10
si_high reg_r11, CPU_r11
si_high reg_r12, CPU_r12
si_high reg_r13, CPU_r13
si_high reg_r14, CPU_r14
si_high reg_r15, CPU_r15
.purgem si_high

.pushsection_rodata
.global NAME(si_gadgets)
NAME(si_gadgets):
.irp reg, REG_LIST
    .irp times, 1,2,4,8
        .quad NAME(gadget_si_\reg\()_\times)
    .endr
.endr
.popsection

.macro si64_low name, reg
    .irp times, 1,2,4,8
        .gadget si64_\name\()_\times
            .ifc \times,1
                add _xaddr, _xaddr, \reg
            .else N .ifc \times,2
                add _xaddr, _xaddr, \reg, lsl 1
            .else N .ifc \times,4
                add _xaddr, _xaddr, \reg, lsl 2
            .else N .ifc \times,8
                add _xaddr, _xaddr, \reg, lsl 3
            .endif N .endif N .endif N .endif
            gret
    .endr
.endm
si64_low reg_a, xax
si64_low reg_c, xcx
si64_low reg_d, xdx
si64_low reg_b, xbx
si64_low reg_sp, xsp
si64_low reg_bp, xbp
si64_low reg_si, xsi
si64_low reg_di, xdi
.purgem si64_low

.macro si64_high name, offset
    .irp times, 1,2,4,8
        .gadget si64_\name\()_\times
            ldr x8, [_cpu, \offset]
            .ifc \times,1
                add _xaddr, _xaddr, x8
            .else N .ifc \times,2
                add _xaddr, _xaddr, x8, lsl 1
            .else N .ifc \times,4
                add _xaddr, _xaddr, x8, lsl 2
            .else N .ifc \times,8
                add _xaddr, _xaddr, x8, lsl 3
            .endif N .endif N .endif N .endif
            gret
    .endr
.endm
si64_high reg_r8, CPU_r8
si64_high reg_r9, CPU_r9
si64_high reg_r10, CPU_r10
si64_high reg_r11, CPU_r11
si64_high reg_r12, CPU_r12
si64_high reg_r13, CPU_r13
si64_high reg_r14, CPU_r14
si64_high reg_r15, CPU_r15
.purgem si64_high

.pushsection_rodata
.global NAME(si64_gadgets)
NAME(si64_gadgets):
.irp reg, REG_LIST
    .irp times, 1,2,4,8
        .quad NAME(gadget_si64_\reg\()_\times)
    .endr
.endr
.popsection

.gadget seg_gs
    ldr w8, [_cpu, CPU_gsbase]
    add _addr, _addr, w8
    gret
.gadget seg_fs
    ldr w8, [_cpu, CPU_fsbase]
    add _addr, _addr, w8
    gret
.gadget seg_gs64
    ldr x8, [_cpu, CPU_gsbase]
    add _xaddr, _xaddr, x8
    gret
.gadget seg_fs64
    ldr x8, [_cpu, CPU_fsbase]
    add _xaddr, _xaddr, x8
    gret

.irp type, read,write

.global handle_\type\()_miss
handle_\type\()_miss:
    save_c
    # %tlb actually points to tlb->entries
    sub x0, _tlb, TLB_entries
    mov x1, _xaddr
    .ifc \type,read
        mov x2, xzr
    .else
        mov x2, 1
    .endif
    bl NAME(tlb_handle_miss)
    mov x19, x0
    restore_c
    cbz x19, segfault_\type
    mov _xaddr, x19
    ret

.global segfault_\type
segfault_\type:
    ldr _xaddr, [_tlb, -TLB_entries+TLB_segfault_addr]
    str _xaddr, [_cpu, CPU_segfault_addr]
    .ifc \type,read
        mov w0, 0
    .else
        mov w0, 1
    .endif
    strb w0, [_cpu, CPU_segfault_was_write]
    ldr x0, [_ip]
    mov eip, w0
    sub w8, esp, 4
    tst x0, 1 << 62
    csel esp, esp, w8, eq
    mov x0, INT_GPF
    b fiber_exit

.endr

.global crosspage_load
crosspage_load:
    save_c
    sub x0, _tlb, TLB_entries
    add x2, _cpu, LOCAL_value
    mov x1, _xaddr
    mov x3, x19
    bl NAME(__tlb_read_cross_page)
    mov x19, x0
    restore_c
    cbz x19, segfault_read
    str _xaddr, [_cpu, LOCAL_value_addr]
    add _xaddr, _cpu, LOCAL_value
    ret

.global crosspage_store
crosspage_store:
    save_c
    sub x0, _tlb, TLB_entries
    add x2, _cpu, LOCAL_value
    ldr x1, [_cpu, LOCAL_value_addr]
    mov x3, x19
    bl NAME(__tlb_write_cross_page)
    mov x19, x0
    restore_c
    cbz x19, segfault_write
    ret
