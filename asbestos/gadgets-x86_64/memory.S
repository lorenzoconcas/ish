#include "gadgets.h"
#include "emu/interrupt.h"

.gadget push
    leal -4(%_esp), %_addr
    write_prep 32, push
    movl %_tmp, (%_addrq)
    write_done 32, push
    sub $4, %_esp
    gret 1
.gadget push64
    leaq -8(%_rsp), %_addrq
    write_prep 64, push64
    movq %tmp, (%_addrq)
    write_done 64, push64
    subq $8, %_rsp
    gret 1
.gadget pop
    movl %_esp, %_addr
    read_prep 32, pop
    movl (%_addrq), %_tmp
    add $4, %_esp
    gret 1
.gadget pop64
    movq %_rsp, %_addrq
    read_prep 64, pop64
    movq (%_addrq), %tmp
    addq $8, %_rsp
    gret 1

.macro x name, reg
    .gadget addr_\name
        movl %\reg, %_addr
        addl (%_ip), %_addr
        gret 1
.endm
.each_reg x
.purgem x
.macro addr_high name, offset
    .gadget addr_\name
        movl \offset(%_cpu), %_addr
        addl (%_ip), %_addr
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
    movl (%_ip), %_addr
    gret 1
.gadget_list addr, REG_LIST

.macro addr64_low name, reg
    .gadget addr64_\name
        movq (%_ip), %_addrq
        addq %\reg, %_addrq
        gret 1
.endm
addr64_low reg_a, rax
addr64_low reg_c, rcx
addr64_low reg_d, rdx
addr64_low reg_b, rbx
addr64_low reg_sp, _rsp
addr64_low reg_bp, rbp
addr64_low reg_si, rsi
addr64_low reg_di, rdi
.purgem addr64_low
.macro addr64_high name, offset
    .gadget addr64_\name
        movq (%_ip), %_addrq
        addq \offset(%_cpu), %_addrq
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
    movq (%_ip), %_addrq
    gret 1
.gadget_list addr64, REG_LIST

.macro x name, reg
    .irp times, 1,2,4,8
        .gadget si_\name\()_\times
            .ifnc \reg,esp
                leal (%_addr,%\reg,\times), %_addr
            .else
                leal (%_addr,%_esp,\times), %_addr
            .endif
            gret
    .endr
.endm
.each_reg x
.purgem x

.macro si_high name, offset
    .irp times, 1,2,4,8
        .gadget si_\name\()_\times
            movl \offset(%_cpu), %r14d
            leal (%_addr,%r14d,\times), %_addr
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
.global.name si_gadgets
.irp reg, REG_LIST
    .irp times, 1,2,4,8
        .quad NAME(gadget_si_\reg\()_\times)
    .endr
.endr
.popsection

.macro si64_low name, reg
    .irp times, 1,2,4,8
        .gadget si64_\name\()_\times
            leaq (%_addrq,%\reg,\times), %_addrq
            gret
    .endr
.endm
si64_low reg_a, rax
si64_low reg_c, rcx
si64_low reg_d, rdx
si64_low reg_b, rbx
si64_low reg_sp, _rsp
si64_low reg_bp, rbp
si64_low reg_si, rsi
si64_low reg_di, rdi
.purgem si64_low

.macro si64_high name, offset
    .irp times, 1,2,4,8
        .gadget si64_\name\()_\times
            movq \offset(%_cpu), %r14
            leaq (%_addrq,%r14,\times), %_addrq
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
.global.name si64_gadgets
.irp reg, REG_LIST
    .irp times, 1,2,4,8
        .quad NAME(gadget_si64_\reg\()_\times)
    .endr
.endr
.popsection

.gadget seg_gs
    addl CPU_gsbase(%_cpu), %_addr
    gret
.gadget seg_fs
    addl CPU_fsbase(%_cpu), %_addr
    gret

.irp type, read,write

.global handle_\type\()_miss
handle_\type\()_miss:
    subq $8, %rsp
    save_c
    # %tlb actually points to tlb->entries
    leaq -TLB_entries(%_tlb), %rdi
    movl %_addr, %esi
    .ifc \type,read
        movq $0, %rdx
    .else
        movq $1, %rdx
    .endif
    call NAME(tlb_handle_miss)
    movq %rax, %r15
    restore_c
    testq %r15, %r15
    jz segfault_\type
    movq %r15, %_addrq
    addq $8, %rsp
    ret

segfault_\type:
    movl -TLB_entries+TLB_segfault_addr(%_tlb), %_addr
    movl %_addr, CPU_segfault_addr(%_cpu)
    .ifc \type,read
        movb $0, CPU_segfault_was_write(%_cpu)
    .else
        movb $1, CPU_segfault_was_write(%_cpu)
    .endif
    movq (%_ip), %_ip
    leal -4(%_esp), %r14d
    btq $62, %_ip
    cmovcl %r14d, %_esp
    movl %_eip, %_eip
    movl $INT_GPF, %_tmp
    jmp fiber_exit

.endr

.global crosspage_load
crosspage_load:
    save_c odd
    leaq -TLB_entries(%_tlb), %rdi
    movl %_addr, %esi
    leaq LOCAL_value(%_cpu), %rdx
    movq %r14, %rcx
    call NAME(__tlb_read_cross_page)
    movq %rax, %r14
    restore_c odd
    testq %r14, %r14
    jz segfault_read
    movl %_addr, LOCAL_value_addr(%_cpu)
    leaq LOCAL_value(%_cpu), %_addrq
    ret

.global crosspage_store
crosspage_store:
    save_c odd
    leaq -TLB_entries(%_tlb), %rdi
    movl LOCAL_value_addr(%_cpu), %esi
    leaq LOCAL_value(%_cpu), %rdx
    movq %r14, %rcx
    call NAME(__tlb_write_cross_page)
    movq %rax, %r14
    restore_c odd
    testq %r14, %r14
    jz segfault_write
    ret
