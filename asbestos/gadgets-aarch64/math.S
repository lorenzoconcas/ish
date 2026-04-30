#include "gadgets.h"
#include "math.h"

.gadget load32_addr
    mov _tmp, _addr
    gret

.gadget load16_gs
    ldrh _tmp, [_cpu, #CPU_gs]
    gret

.gadget store16_gs
    strh _tmp, [_cpu, #CPU_gs]
    gret

# this would have been just a few nice compact nested loops, but gas said "nuh uh"

.macro _do_op op, arg, size, s
    .ifc \op,load
        movs _tmp, \arg, \s
        uxts _tmp, _tmp, \s
        .exitm
    .else N .ifc \op,store
        movs \arg, _tmp, \s
        .exitm
    .endif N .endif

    .ifin(\op, add,sub,adc,sbc)
        setf_a \arg, _tmp
    .endifin
    .ifin(\op, and,orr,eor)
        clearf_a
        clearf_oc
    .endifin
    .ifin(\op, adc,sbc)
        ldrb w10, [_cpu, CPU_cf]
        .ifc \op,adc
            cmp w10, 1
        .else
            mvn w10, w10
            cmn w10, 1
        .endif
    .endifin

    .ifin(\op, and,orr,eor)
        \op _tmp, _tmp, \arg
    .endifin

    .ifin(\op, add,sub,adc,sbc)
        do_add \op, _tmp, \arg, \s
    .endifin

    .ifc \op,imul
        .ifnb \s
            sxt\s w10, \arg
            mul _tmp, _tmp, w10
            cmp _tmp, _tmp, sxt\s
        .else
            smull _xtmp, _tmp, \arg
            cmp _xtmp, _tmp, sxtw
        .endif
        cset w10, ne
        strb w10, [_cpu, CPU_cf]
        strb w10, [_cpu, CPU_of]
    .endif

    .ifin(\op, bsf,bsr)
        .ifnb \s
            uxt\s w10, \arg
        .else
            mov w10, \arg
        .endif
        .ifc \op,bsf
            .if \size != 32
                orr w10, w10, 1<<\size
            .endif
            rbit w10, w10
            clz w10, w10
            cmp w10, \size
        .else
            clz w10, w10
            .if \size != 32
                sub w10, w10, 32-\size
            .endif
            cmp w10, \size
            mov w9, \size-1
            sub w10, w9, w10
        .endif
        csel _tmp, w10, _tmp, ne
        cset w10, eq
        ldrb w9, [_cpu, CPU_eflags]
        bic w9, w9, ZF_FLAG
        orr w9, w9, w10, lsl 6
        strb w9, [_cpu, CPU_eflags]
        ldrb w9, [_cpu, CPU_flags_res]
        bic w9, w9, ZF_RES
        strb w9, [_cpu, CPU_flags_res]
    .endifin

    .ifc \op,xchg
        mov w9, _tmp
        mov _tmp, \arg
        movs \arg, w9, \s
    .endif

    .ifin(\op, add,sub,adc,sbc,and,orr,eor)
        setf_zsp \s
    .endifin
.endm
.macro do_op op, size, arg
    ss \size, _do_op, \op, \arg
.endm

.macro do_reg_op op, armop, size, reg
    .gadget \op\size\()_reg_\reg
        do_op \armop, \size, e\reg\()x
        gret
.endm

.macro do_hi_op op, size, reg
    ubfx w12, e\reg\()x, 8, 8
    do_op \op, \size, w12
    bfi e\reg\()x, w12, 8, 8
.endm

.macro do_high_reg_op op, armop, size, name, offset
    .gadget \op\size\()_reg_\name
        ldr x12, [_cpu, \offset]
        mov x13, x12
        do_op \armop, \size, w12
        .ifin(\op, store,xchg)
            .if \size == 8
                bfi x13, x12, 0, 8
                str x13, [_cpu, \offset]
            .elseif \size == 16
                bfi x13, x12, 0, 16
                str x13, [_cpu, \offset]
            .else
                uxtw x12, w12
                str x12, [_cpu, \offset]
            .endif
        .endifin
        gret
.endm

.macro do_op_size op, armop, size, s
    .ifnc \op,store
        .gadget \op\size\()_imm
            ldr\s w8, [_ip]
            do_op \armop, \size, w8
            gret 1
    .endif

    .ifnc \op,xchg
        .gadget \op\size\()_mem
            .ifc \op,store
                write_prep \size, \op\size\()_mem
            .else N .ifc \op,xchg
                write_prep \size, \op\size\()_mem
            .else
                read_prep \size, \op\size\()_mem
            .endif N .endif
            ldr\s w8, [_xaddr]
            do_op \armop, \size, w8
            .ifc \op,store
                str\s w8, [_xaddr]
                write_done \size, \op\size\()_mem
            .endif
            gret 1
            .ifc \op,store
                write_bullshit \size, \op\size\()_mem
            .else N .ifc \op,xchg
                write_bullshit \size, \op\size\()_mem
            .else
                read_bullshit \size, \op\size\()_mem
            .endif N .endif
    .else
        # xchg must be atomic
        .gadget \op\size\()_mem
            write_prep \size, \op\size\()_mem
        1:
            ldaxr\s w8, [_xaddr]
            stlxr\s w10, _tmp, [_xaddr]
            cbnz w10, 1b
            movs _tmp, w8
            write_done \size, \op\size\()_mem
            gret 1
            write_bullshit \size, \op\size\()_mem
    .endif

    .irp reg, a,b,c,d
        do_reg_op \op, \armop, \size, \reg
    .endr

    .irp reg, si,di,sp,bp
        .gadget \op\size\()_reg_\reg
            .if \size == 8
                .ifc \reg,sp N do_hi_op \armop, \size, a N .else
                .ifc \reg,bp N do_hi_op \armop, \size, c N .else
                .ifc \reg,si N do_hi_op \armop, \size, d N .else
                .ifc \reg,di N do_hi_op \armop, \size, b
                .endif N .endif N .endif N .endif
            .else
                do_op \armop, \size, e\reg
            .endif
            gret
    .endr

    .if \size == 8
        .gadget \op\size\()_reg_spl
            do_op \armop, \size, esp
            gret
        .gadget \op\size\()_reg_bpl
            do_op \armop, \size, ebp
            gret
        .gadget \op\size\()_reg_sil
            do_op \armop, \size, esi
            gret
        .gadget \op\size\()_reg_dil
            do_op \armop, \size, edi
            gret
    .endif

    do_high_reg_op \op, \armop, \size, r8, CPU_r8
    do_high_reg_op \op, \armop, \size, r9, CPU_r9
    do_high_reg_op \op, \armop, \size, r10, CPU_r10
    do_high_reg_op \op, \armop, \size, r11, CPU_r11
    do_high_reg_op \op, \armop, \size, r12, CPU_r12
    do_high_reg_op \op, \armop, \size, r13, CPU_r13
    do_high_reg_op \op, \armop, \size, r14, CPU_r14
    do_high_reg_op \op, \armop, \size, r15, CPU_r15

.endm

.irp op, load,store,xchg,add,sub,adc,sbb,and,or,xor
    .irp size, SIZE_LIST
        # a couple operations have slightly different names on arm
        .ifc \op,xor
            ss \size, do_op_size, \op, eor
        .else N .ifc \op,sbb
            ss \size, do_op_size, \op, sbc
        .else N .ifc \op,or
            ss \size, do_op_size, \op, orr
        .else
            ss \size, do_op_size, \op, \op
        .endif N .endif N .endif
    .endr
.endr

.macro do_op64 op, arg
    .ifc \op,load
        mov _xtmp, \arg
        .exitm
    .else N .ifc \op,store
        mov \arg, _xtmp
        .exitm
    .else N .ifc \op,xchg
        mov x9, _xtmp
        mov _xtmp, \arg
        mov \arg, x9
        .exitm
    .else N .ifc \op,add
        setf_a64 \arg, _xtmp
        adds _xtmp, _xtmp, \arg
        cset w10, vs
        strb w10, [_cpu, CPU_of]
        cset w10, cs
        strb w10, [_cpu, CPU_cf]
        setf_zsp x, val=_xtmp
        .exitm
    .else N .ifc \op,sub
        setf_a64 \arg, _xtmp
        subs _xtmp, _xtmp, \arg
        cset w10, vs
        strb w10, [_cpu, CPU_of]
        cset w10, cc
        strb w10, [_cpu, CPU_cf]
        setf_zsp x, val=_xtmp
        .exitm
    .else N .ifc \op,adc
        setf_a64 \arg, _xtmp
        ldrb w10, [_cpu, CPU_cf]
        cmp w10, 1
        adcs _xtmp, _xtmp, \arg
        cset w10, vs
        strb w10, [_cpu, CPU_of]
        cset w10, cs
        strb w10, [_cpu, CPU_cf]
        setf_zsp x, val=_xtmp
        .exitm
    .else N .ifc \op,sbc
        setf_a64 \arg, _xtmp
        ldrb w10, [_cpu, CPU_cf]
        mvn w10, w10
        cmn w10, 1
        sbcs _xtmp, _xtmp, \arg
        cset w10, vs
        strb w10, [_cpu, CPU_of]
        cset w10, cc
        strb w10, [_cpu, CPU_cf]
        setf_zsp x, val=_xtmp
        .exitm
    .else N .ifc \op,and
        clearf_a
        clearf_oc
        and _xtmp, _xtmp, \arg
        setf_zsp x, val=_xtmp
        .exitm
    .else N .ifc \op,orr
        clearf_a
        clearf_oc
        orr _xtmp, _xtmp, \arg
        setf_zsp x, val=_xtmp
        .exitm
    .else N .ifc \op,eor
        clearf_a
        clearf_oc
        eor _xtmp, _xtmp, \arg
        setf_zsp x, val=_xtmp
        .exitm
    .else N .ifc \op,imul
        smulh x9, _xtmp, \arg
        mul _xtmp, _xtmp, \arg
        cmp x9, _xtmp, asr 63
        cset w10, ne
        strb w10, [_cpu, CPU_cf]
        strb w10, [_cpu, CPU_of]
        .exitm
    .else N .ifc \op,bsf
        rbit x10, \arg
        clz x10, x10
        cmp \arg, 0
        csel _xtmp, x10, _xtmp, ne
        cset w10, eq
        ldr w9, [_cpu, CPU_eflags]
        bic w9, w9, ZF_FLAG
        orr w9, w9, w10, lsl 6
        str w9, [_cpu, CPU_eflags]
        ldr w9, [_cpu, CPU_flags_res]
        bic w9, w9, ZF_RES
        str w9, [_cpu, CPU_flags_res]
        .exitm
    .else N .ifc \op,bsr
        clz x11, \arg
        mov x10, 63
        sub x10, x10, x11
        cmp \arg, 0
        csel _xtmp, x10, _xtmp, ne
        cset w10, eq
        ldr w9, [_cpu, CPU_eflags]
        bic w9, w9, ZF_FLAG
        orr w9, w9, w10, lsl 6
        str w9, [_cpu, CPU_eflags]
        ldr w9, [_cpu, CPU_flags_res]
        bic w9, w9, ZF_RES
        str w9, [_cpu, CPU_flags_res]
        .exitm
    .endif N .endif N .endif N .endif N .endif N .endif N .endif N .endif N .endif N .endif N .endif N .endif N .endif
.endm

.macro do_reg_op64 op, armop, name, reg
    .gadget \op\()64_reg_\name
        do_op64 \armop, \reg
        gret
.endm

.macro do_high_reg_op64 op, armop, name, offset
    .gadget \op\()64_reg_\name
        ldr x8, [_cpu, \offset]
        do_op64 \armop, x8
        .ifin(\op, store,xchg)
            str x8, [_cpu, \offset]
        .endifin
        gret
.endm

.macro do_op_size64 op, armop
    .ifnc \op,store
        .gadget \op\()64_imm
            ldr x8, [_ip]
            do_op64 \armop, x8
            gret 1
    .endif

    .gadget \op\()64_mem
        .ifin(\op, store,xchg)
            write_prep 64, \op\()64_mem
        .endifin
        .ifin(\op, load,add,sub,adc,sbb,and,or,xor,imul,bsf,bsr)
            read_prep 64, \op\()64_mem
        .endifin
        .ifnc \op,store
            ldr x8, [_xaddr]
        .endif
        do_op64 \armop, x8
        .ifin(\op, store,xchg)
            str x8, [_xaddr]
            write_done 64, \op\()64_mem
        .endifin
        gret 1
        .ifin(\op, store,xchg)
            write_bullshit 64, \op\()64_mem
        .endifin
        .ifin(\op, load,add,sub,adc,sbb,and,or,xor,imul,bsf,bsr)
            read_bullshit 64, \op\()64_mem
        .endifin

    do_reg_op64 \op, \armop, a, xax
    do_reg_op64 \op, \armop, b, xbx
    do_reg_op64 \op, \armop, c, xcx
    do_reg_op64 \op, \armop, d, xdx
    do_reg_op64 \op, \armop, si, xsi
    do_reg_op64 \op, \armop, di, xdi
    do_reg_op64 \op, \armop, bp, xbp
    do_reg_op64 \op, \armop, sp, xsp
    do_high_reg_op64 \op, \armop, r8, CPU_r8
    do_high_reg_op64 \op, \armop, r9, CPU_r9
    do_high_reg_op64 \op, \armop, r10, CPU_r10
    do_high_reg_op64 \op, \armop, r11, CPU_r11
    do_high_reg_op64 \op, \armop, r12, CPU_r12
    do_high_reg_op64 \op, \armop, r13, CPU_r13
    do_high_reg_op64 \op, \armop, r14, CPU_r14
    do_high_reg_op64 \op, \armop, r15, CPU_r15
.endm

.gadget load64_addr
    mov _xtmp, _xaddr
    gret

.irp op, load,store,xchg,add,sub,adc,sbb,and,or,xor,imul,bsf,bsr
    .ifc \op,xor
        do_op_size64 \op, eor
    .else N .ifc \op,sbb
        do_op_size64 \op, sbc
    .else N .ifc \op,or
        do_op_size64 \op, orr
    .else
        do_op_size64 \op, \op
    .endif N .endif N .endif
.endr

.irp op, load,store,xchg,add,sub,adc,sbb,and,or,xor
    .gadget_array \op
.endr

.irp op, imul,bsf,bsr
    .irp size, 16,32
        ss \size, do_op_size, \op, \op
    .endr
    .gadget_array \op
.endr

# atomics. oof

.macro do_op_size_atomic opname, op, size, s
    .gadget atomic_\opname\size\()_mem
        # There's so much stuff going on inside most of these operations that
        # the implementation is a compare-and-swap loop, instead of just ldaxr/stlxr
        write_prep \size, atomic_\opname\size\()_mem
        ldr\s w12, [_xaddr]
    1:
        mov w8, w12

        # do the operation
        # dest = w8, src = _tmp
        .ifin(\op, add,sub,adc,sbc)
            setf_a src=_tmp, dst=w8
        .endifin
        .ifin(\op, and,orr,eor)
            clearf_a
            clearf_oc
        .endifin
        .ifin(\op, adc,sbc)
            ldrb w10, [_cpu, CPU_cf]
            .ifc \op,adc
                cmp w10, 1
            .else
                mvn w10, w10
                cmn w10, 1
            .endif
        .endifin

        .ifin(\op, and,orr,eor)
            \op w8, w8, _tmp
        .endifin
        .ifin(\op, add,sub,adc,sbc)
            do_add \op, w8, _tmp, \s
        .endifin
        .ifc \op,xadd
            # exchange, then add
            mov w9, w8
            mov w8, _tmp
            do_add add, w8, w9, \s
        .endif

        .ifin(\op, add,sub,adc,sbc,and,orr,eor,xadd)
            setf_zsp \s, val=w8
        .endifin

        .ifin(\op, inc,dec)
            mov w10, 1
            setf_a src=w10, dst=w8
            .ifb \s
                .ifc \op,inc
                    adds w8, w8, 1
                .else
                    subs w8, w8, 1
                .endif
                cset w9, vs
            .else
                sxt\s w8, w8
                .ifc \op,inc
                    adds w8, w8, 1
                .else
                    subs w8, w8, 1
                .endif
                cmp w8, w8, sxt\s
                cset w9, ne
            .endif
            strb w9, [_cpu, CPU_of]
            setf_zsp \s, val=w8
        .endifin

    2:
        ldaxr\s w13, [_xaddr]
        cmp w12, w13
        b.ne 3f
        stlxr\s w13, w8, [_xaddr]
        cbnz w13, 2b
        .ifc \op,xadd
            mov _tmp, w9
        .endif
        write_done \size, atomic_\opname\size\()_mem
        gret 1
        write_bullshit \size, atomic_\opname\size\()_mem
    3:
        dmb ish
        mov w12, w13
        b 1b
.endm

.macro do_op_size_atomic64 opname, op
    .gadget atomic_\opname\()64_mem
        # 64-bit atomics use the same compare-and-swap shape as the smaller
        # operations, but keep the old value and result in x registers.
        write_prep 64, atomic_\opname\()64_mem
        ldr x12, [_xaddr]
    1:
        mov x8, x12

        .ifin(\op, add,sub,adc,sbc)
            setf_a64 _xtmp, x8
        .endifin
        .ifin(\op, and,orr,eor)
            clearf_a
            clearf_oc
        .endifin
        .ifin(\op, adc,sbc)
            ldrb w10, [_cpu, CPU_cf]
            .ifc \op,adc
                cmp w10, 1
            .else
                mvn w10, w10
                cmn w10, 1
            .endif
        .endifin

        .ifin(\op, and,orr,eor)
            \op x8, x8, _xtmp
        .endifin
        .ifc \op,add
            adds x8, x8, _xtmp
            cset w10, vs
            strb w10, [_cpu, CPU_of]
            cset w10, cs
            strb w10, [_cpu, CPU_cf]
        .endif
        .ifc \op,sub
            subs x8, x8, _xtmp
            cset w10, vs
            strb w10, [_cpu, CPU_of]
            cset w10, cc
            strb w10, [_cpu, CPU_cf]
        .endif
        .ifc \op,adc
            adcs x8, x8, _xtmp
            cset w10, vs
            strb w10, [_cpu, CPU_of]
            cset w10, cs
            strb w10, [_cpu, CPU_cf]
        .endif
        .ifc \op,sbc
            sbcs x8, x8, _xtmp
            cset w10, vs
            strb w10, [_cpu, CPU_of]
            cset w10, cc
            strb w10, [_cpu, CPU_cf]
        .endif
        .ifc \op,xadd
            mov x9, x8
            mov x8, _xtmp
            adds x8, x8, x9
            cset w10, vs
            strb w10, [_cpu, CPU_of]
            cset w10, cs
            strb w10, [_cpu, CPU_cf]
        .endif

        .ifin(\op, add,sub,adc,sbc,and,orr,eor,xadd)
            setf_zsp x, val=x8
        .endifin

        .ifin(\op, inc,dec)
            mov x10, 1
            setf_a64 x10, x8
            .ifc \op,inc
                adds x8, x8, 1
            .else
                subs x8, x8, 1
            .endif
            cset w9, vs
            strb w9, [_cpu, CPU_of]
            setf_zsp x, val=x8
        .endifin

    2:
        ldaxr x13, [_xaddr]
        cmp x12, x13
        b.ne 3f
        stlxr w13, x8, [_xaddr]
        cbnz w13, 2b
        .ifc \op,xadd
            mov _xtmp, x9
        .endif
        write_done 64, atomic_\opname\()64_mem
        gret 1
        write_bullshit 64, atomic_\opname\()64_mem
    3:
        dmb ish
        mov x12, x13
        b 1b
.endm

.irp op, add,sub,adc,sbb,and,or,xor,inc,dec,xadd
    .irp size, SIZE_LIST
        .ifc \op,xor
            ss \size, do_op_size_atomic, \op, eor
        .else N .ifc \op,sbb
            ss \size, do_op_size_atomic, \op, sbc
        .else N .ifc \op,or
            ss \size, do_op_size_atomic, \op, orr
        .else
            ss \size, do_op_size_atomic, \op, \op
        .endif N .endif N .endif
    .endr
    .ifc \op,xor
        do_op_size_atomic64 \op, eor
    .else N .ifc \op,sbb
        do_op_size_atomic64 \op, sbc
    .else N .ifc \op,or
        do_op_size_atomic64 \op, orr
    .else
        do_op_size_atomic64 \op, \op
    .endif N .endif N .endif
    .gadget_array atomic_\op
.endr

# unary operations (well, only one explicit operand)

.macro do_inc size, s
    .if \size == 64
        mov x10, 1
        setf_a64 x10, _xtmp
        adds _xtmp, _xtmp, 1
        cset w8, vs
        strb w8, [_cpu, CPU_of]
        setf_zsp x, val=_xtmp
    .else
        mov w10, 1
        setf_a w10, _tmp
        .ifb \s
        adds _tmp, _tmp, 1
        cset w8, vs
        .else
        sxt\s _tmp, _tmp
        add _tmp, _tmp, 1
        cmp _tmp, _tmp, sxt\s
        cset w8, ne
        .endif
        strb w8, [_cpu, CPU_of]
        setf_zsp \s
    .endif
.endm
.macro do_dec size, s
    .if \size == 64
        mov x10, 1
        setf_a64 x10, _xtmp
        subs _xtmp, _xtmp, 1
        cset w8, vs
        strb w8, [_cpu, CPU_of]
        setf_zsp x, val=_xtmp
    .else
        mov w10, 1
        setf_a w10, _tmp
        .ifb \s
        subs _tmp, _tmp, 1
        cset w8, vs
        .else
        sxt\s _tmp, _tmp
        sub _tmp, _tmp, 1
        cmp _tmp, _tmp, sxt\s
        cset w8, ne
        .endif
        strb w8, [_cpu, CPU_of]
        setf_zsp \s
    .endif
.endm

.macro do_sign_extend size, s
    .if \size == 8
        sxtb _xtmp, _tmp
    .elseif \size == 16
        sxth _xtmp, _tmp
    .elseif \size == 32
        sxtw _xtmp, _tmp
    .endif
.endm
.macro do_zero_extend size, s
    .if \size != 32
        uxt\s _tmp, _tmp
    .endif
.endm
.macro do_div size, s
    .if \size == 8
        uxth w8, eax
        uxtb _tmp, _tmp
        udiv w9, w8, _tmp
        msub w10, w9, _tmp, w8
        bfi eax, w9, 0, 8
        bfi eax, w10, 8, 8
    .elseif \size == 16
        bfi w8, eax, 0, 16
        bfi w8, edx, 16, 16
        uxth _tmp, _tmp
        udiv w9, w8, _tmp
        msub w10, w9, _tmp, w8
        bfi eax, w9, 0, 16
        bfi edx, w10, 0, 16
    .elseif \size == 32
        bfi x8, xax, 0, 32
        bfi x8, xdx, 32, 32
        uxtw _xtmp, _tmp
        udiv x9, x8, _xtmp
        msub x10, x9, _xtmp, x8
        mov eax, w9
        mov edx, w10
    .elseif \size == 64
        save_regs
        save_c
        mov x2, _xtmp
        mov x0, _cpu
        mov x1, x2
        bl NAME(helper_div64)
        restore_c
        load_regs
    .endif
.endm
.macro do_idiv size, s
    # another lazy ass copy paste job
    .if \size == 8
        sxth w8, eax
        sxtb _tmp, _tmp
        sdiv w9, w8, _tmp
        msub w10, w9, _tmp, w8
        bfi eax, w9, 0, 8
        bfi eax, w10, 8, 8
    .elseif \size == 16
        bfi w8, eax, 0, 16
        bfi w8, edx, 16, 16
        sxth _tmp, _tmp
        sdiv w9, w8, _tmp
        msub w10, w9, _tmp, w8
        bfi eax, w9, 0, 16
        bfi edx, w10, 0, 16
    .elseif \size == 32
        bfi x8, xax, 0, 32
        bfi x8, xdx, 32, 32
        sxtw _xtmp, _tmp
        sdiv x9, x8, _xtmp
        msub x10, x9, _xtmp, x8
        mov eax, w9
        mov edx, w10
    .elseif \size == 64
        save_regs
        save_c
        mov x2, _xtmp
        mov x0, _cpu
        mov x1, x2
        bl NAME(helper_idiv64)
        restore_c
        load_regs
    .endif
.endm
.macro do_mul size, s
    .if \size == 64
        umulh xdx, xax, _xtmp
        mul xax, xax, _xtmp
        cmp xdx, 0
    .else N .ifb \s
        umull xax, eax, _tmp
        lsr xdx, xax, 32
        cmp xax, eax, uxtw
    .else
        uxt\s w8, eax
        uxt\s _tmp, _tmp
        mul w8, w8, _tmp
        cmp w8, w8, uxt\s
        .if \size == 8
            bfxil eax, w8, 0, \size*2
        .else
            bfxil eax, w8, 0, \size
            bfxil edx, w8, \size, \size
        .endif
    .endif N .endif
    cset w8, ne
    strb w8, [_cpu, CPU_cf]
    strb w8, [_cpu, CPU_of]
.endm
.macro do_imul1 size, s
    .if \size == 64
        smulh xdx, xax, _xtmp
        mul xax, xax, _xtmp
        cmp xdx, xax, asr 63
    .else N .ifb \s
        smull xax, eax, _tmp
        lsr xdx, xax, 32
        cmp xax, eax, sxtw
    .else
        sxt\s w8, eax
        sxt\s _tmp, _tmp
        mul w8, w8, _tmp
        cmp w8, w8, sxt\s
        .if \size == 8
            bfxil eax, w8, 0, \size*2
        .else
            bfxil eax, w8, 0, \size
            bfxil edx, w8, \size, \size
        .endif
    .endif N .endif
    cset w8, ne
    strb w8, [_cpu, CPU_cf]
    strb w8, [_cpu, CPU_of]
.endm
.macro do_not size, s
    .if \size == 64
        mvn _xtmp, _xtmp
    .else
        .ifb \s
            mvn _tmp, _tmp
        .else
        movs w10, _tmp, \s
        mvn w10, w10
        movs _tmp, w10, \s
        .endif
    .endif
.endm

.irp op, inc,dec,not
    .irp size, 8,16,32,64
        .gadget \op\()_\size
            ss \size, do_\op
            gret
    .endr
    .gadget_list \op, 8,16,32,64
.endr

.irp op, sign_extend,zero_extend
    .irp size, SIZE_LIST
        .gadget \op\()_\size
            ss \size, do_\op
            gret
    .endr
    .gadget_list \op, SIZE_LIST
.endr

.irp op, div,idiv
    .irp size, SIZE_LIST,64
        .gadget \op\()_\size
            ss \size, do_\op
            gret
    .endr
    .gadget_list \op, SIZE_LIST,64
.endr

.irp op, mul,imul1
    .irp size, SIZE_LIST,64
        .gadget \op\()_\size
            ss \size, do_\op
            gret
    .endr
    .gadget_list \op, SIZE_LIST,64
.endr

.gadget cvt_16
    tst eax, 0x8000
    cinv w8, wzr, ne
    bfxil edx, w8, 0, 16
    gret
.gadget cvt_32
    tst eax, 0x80000000
    cinv edx, wzr, ne
    gret
.gadget cvt_64
    asr xdx, xax, 63
    gret
.gadget_list cvt, 8,16,32,64

.gadget cvte_16
    sxtb w8, eax
    bfxil eax, w8, 0, 16
    gret
.gadget cvte_32
    sxth eax, eax
    gret
.gadget cvte_64
    sxtw xax, eax
    gret
.gadget_list cvte, 8,16,32,64
