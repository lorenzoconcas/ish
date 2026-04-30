#include "gadgets.h"

.macro do_shift type, size, s
    .irp arg, reg_c,imm
        .gadget \type\size\()_\arg
            .ifc \arg,imm
                ldr w8, [_ip]
                ands w8, w8, 31
            .else
                ands w8, ecx, 31
            .endif
            b.eq 1f

            # shift by one less, then by one more
            # that way we can retrieve the last bit shifted out for calculating CF and OF
            .ifc \type,shl
                sub w8, w8, 1
                lsl _tmp, _tmp, w8
                ubfx w9, _tmp, \size-1, 1
                ubfx w10, _tmp, \size-2, 1
                lsl _tmp, _tmp, 1
                eor w10, w10, w9
                strb w9, [_cpu, CPU_cf]
                strb w10, [_cpu, CPU_of]
            .endif
            .ifc \type,shr
                ubfx w10, _tmp, \size-1, 1
                sub w8, w8, 1
                lsr _tmp, _tmp, w8
                and w9, _tmp, 1
                lsr _tmp, _tmp, 1
                strb w9, [_cpu, CPU_cf]
                strb w10, [_cpu, CPU_of]
            .endif
            .ifc \type,sar
                # lazy ass copy paste job
                .ifnb \s
                    sxt\s _tmp, _tmp
                .endif
                sub w8, w8, 1
                asr _tmp, _tmp, w8
                and w9, _tmp, 1
                asr _tmp, _tmp, 1
                strb w9, [_cpu, CPU_cf]
                strb wzr, [_cpu, CPU_of]
            .endif

            # regrets
            .ifin(\type, rol,ror)
                .ifb \s
                    .ifc \type,rol
                        neg w8, w8
                    .endif
                    ror _tmp, _tmp, w8
                .else
                    # kill me
                    uxt\s _tmp, _tmp
                    neg w9, w8
                    and w8, w8, \size-1
                    and w9, w9, \size-1
                    .ifc \type,rol
                        lsl w8, _tmp, w8
                        lsr w9, _tmp, w9
                    .else
                        lsr w8, _tmp, w8
                        lsl w9, _tmp, w9
                    .endif
                    orr _tmp, w8, w9
                .endif
                .ifc \type,rol
                    ubfx w9, _tmp, 0, 1
                    ubfx w10, _tmp, \size-1, 1
                .else
                    ubfx w9, _tmp, \size-1, 1
                    ubfx w10, _tmp, \size-2, 1
                .endif
                eor w10, w10, w9
                strb w9, [_cpu, CPU_cf]
                strb w10, [_cpu, CPU_of]
            .endifin

            # aaaaaaaaaaaaaa
            .ifin(\type, rcl,rcr)
                .ifc \type,rcr
                    ubfx w9, _tmp, \size-1, 1
                    ldrb w10, [_cpu, CPU_cf]
                    eor w9, w9, w10
                    strb w9, [_cpu, CPU_of]
                .endif

                ldrb w9, [_cpu, CPU_cf]
                lsl x9, x9, \size
                orr _xtmp, _xtmp, x9
                # so ok we mask the shift count, not too hard
                and w8, w8, 31
                # ...now mod by \size+1 oof
                .if \size == 8
                    .irpc _, 123
                        subs w10, w8, \size+1
                        csel w8, w10, w8, gt
                    .endr
                .elseif \size == 16
                    subs w10, w8, \size+1
                    csel w8, w10, w8, gt
                .endif
                mov w9, \size+1
                sub w9, w9, w8
                .ifc \type,rcl
                    lsl x8, _xtmp, x8
                    lsr x9, _xtmp, x9
                .else
                    lsr x8, _xtmp, x8
                    lsl x9, _xtmp, x9
                .endif
                orr _xtmp, x8, x9
                ubfx x9, _xtmp, \size, 1
                strb w9, [_cpu, CPU_cf]
                .ifc \type,rcl
                    ubfx w10, _tmp, \size-1, 1
                    eor w10, w10, w9
                    strb w10, [_cpu, CPU_of]
                .endif
            .endifin

            .ifin(\type, shl,shr,sar)
                setf_zsp \s
                clearf_a
            .endifin
        1:
            .ifc \arg,imm
                gret 1
            .else
                gret
            .endif
    .endr
.endm

.irp type, shl,shr,sar,rol,ror,rcl,rcr
    .irp size, 8,16,32
        ss \size, do_shift, \type
    .endr
.endr

.macro do_shift64 type
    .irp arg, reg_c,imm
        .gadget \type\()64_\arg
            .ifc \arg,imm
                ldr w8, [_ip]
                and w8, w8, 63
            .else
                and w8, ecx, 63
            .endif
            cbz w8, 1f
            .ifc \type,shl
                sub w8, w8, 1
                lsl _xtmp, _xtmp, 1
                ubfx x9, _xtmp, 63, 1
                ubfx x10, _xtmp, 62, 1
                lslv _xtmp, _xtmp, x8
                eor x10, x10, x9
                strb w9, [_cpu, CPU_cf]
                strb w10, [_cpu, CPU_of]
            .endif
            .ifc \type,shr
                ubfx x10, _xtmp, 63, 1
                sub w8, w8, 1
                lsr _xtmp, _xtmp, 1
                and x9, _xtmp, 1
                lsrv _xtmp, _xtmp, x8
                strb w9, [_cpu, CPU_cf]
                strb w10, [_cpu, CPU_of]
            .endif
            .ifc \type,sar
                sub w8, w8, 1
                asr _xtmp, _xtmp, 1
                and x9, _xtmp, 1
                asrv _xtmp, _xtmp, x8
                strb w9, [_cpu, CPU_cf]
                strb wzr, [_cpu, CPU_of]
            .endif
            .ifin(\type, rol,ror)
                mov w9, 64
                sub w9, w9, w8
                .ifc \type,rol
                    lsl x10, _xtmp, x8
                    lsr x11, _xtmp, x9
                .else
                    lsr x10, _xtmp, x8
                    lsl x11, _xtmp, x9
                .endif
                orr _xtmp, x10, x11
                .ifc \type,rol
                    ubfx x9, _xtmp, 0, 1
                    ubfx x10, _xtmp, 63, 1
                .else
                    ubfx x9, _xtmp, 63, 1
                    ubfx x10, _xtmp, 62, 1
                .endif
                eor x10, x10, x9
                strb w9, [_cpu, CPU_cf]
                strb w10, [_cpu, CPU_of]
            .endifin
            .ifin(\type, shl,shr,sar)
                setf_zsp x, val=_xtmp
                clearf_a
            .endifin
        1:
            .ifc \arg,imm
                gret 1
            .else
                gret
            .endif
    .endr
.endm

.irp type, shl,shr,sar,rol,ror
    do_shift64 \type
.endr

.irp type, shl,shr,sar,rol,ror,rcl,rcr
    .gadget_array \type
.endr

.macro do_shiftd op, arg
    .macro x name, reg
        .gadget \op\()_\arg\()32_\name
            .ifc \arg,imm
                ldrb w8, [_ip]
            .else
                uxtb w8, ecx
            .endif
            tst w8, 31
            b.eq 1f
            mov w9, 32
            sub w9, w9, w8
            sub w8, w8, 1 /* shift by one less */
            .ifc \op,shrd
                lsr w8, _tmp, w8
                # and by one more
                and w10, w8, 1
                lsr w8, w8, 1
                lsl w9, \reg, w9
            .else
                lsl w8, _tmp, w8
                # and by one more
                ubfx w10, w8, 31, 1
                lsl w8, w8, 1
                lsr w9, \reg, w9
            .endif
            orr _tmp, w8, w9
            strb w10, [_cpu, CPU_cf]
            setf_zsp
        1:
            .ifc \arg,imm
                gret 1
            .else
                gret
            .endif
    .endm
    .each_reg x
    .purgem x
.endm
.irp op, shrd,shld
    .irp arg, imm,cl
        do_shiftd \op, \arg
    .endr
.endr

.macro do_shiftd64_one op, arg, name, reg
    .gadget \op\()_\arg\()64_\name
        .ifc \arg,imm
            ldrb w8, [_ip]
        .else
            uxtb w8, ecx
        .endif
        and w8, w8, 63
        cbz w8, 1f
        mov w9, 64
        sub w9, w9, w8
        sub w8, w8, 1 /* shift by one less */
        .ifc \op,shrd
            lsr x10, _xtmp, x8
            and x12, x10, 1
            lsr x10, x10, 1
            lsl x11, \reg, x9
        .else
            lsl x10, _xtmp, x8
            ubfx x12, x10, 63, 1
            lsl x10, x10, 1
            lsr x11, \reg, x9
        .endif
        orr _xtmp, x10, x11
        strb w12, [_cpu, CPU_cf]
        setf_zsp x, val=_xtmp
    1:
        .ifc \arg,imm
            gret 1
        .else
            gret
        .endif
.endm

.macro do_shiftd64_high op, arg, name, offset
    .gadget \op\()_\arg\()64_\name
        ldr x13, [_cpu, \offset]
        .ifc \arg,imm
            ldrb w8, [_ip]
        .else
            uxtb w8, ecx
        .endif
        and w8, w8, 63
        cbz w8, 1f
        mov w9, 64
        sub w9, w9, w8
        sub w8, w8, 1 /* shift by one less */
        .ifc \op,shrd
            lsr x10, _xtmp, x8
            and x12, x10, 1
            lsr x10, x10, 1
            lsl x11, x13, x9
        .else
            lsl x10, _xtmp, x8
            ubfx x12, x10, 63, 1
            lsl x10, x10, 1
            lsr x11, x13, x9
        .endif
        orr _xtmp, x10, x11
        strb w12, [_cpu, CPU_cf]
        setf_zsp x, val=_xtmp
    1:
        .ifc \arg,imm
            gret 1
        .else
            gret
        .endif
.endm

.macro do_shiftd64 op, arg
    do_shiftd64_one \op, \arg, reg_a, xax
    do_shiftd64_one \op, \arg, reg_c, xcx
    do_shiftd64_one \op, \arg, reg_d, xdx
    do_shiftd64_one \op, \arg, reg_b, xbx
    do_shiftd64_one \op, \arg, reg_sp, xsp
    do_shiftd64_one \op, \arg, reg_bp, xbp
    do_shiftd64_one \op, \arg, reg_si, xsi
    do_shiftd64_one \op, \arg, reg_di, xdi
    do_shiftd64_high \op, \arg, reg_r8, CPU_r8
    do_shiftd64_high \op, \arg, reg_r9, CPU_r9
    do_shiftd64_high \op, \arg, reg_r10, CPU_r10
    do_shiftd64_high \op, \arg, reg_r11, CPU_r11
    do_shiftd64_high \op, \arg, reg_r12, CPU_r12
    do_shiftd64_high \op, \arg, reg_r13, CPU_r13
    do_shiftd64_high \op, \arg, reg_r14, CPU_r14
    do_shiftd64_high \op, \arg, reg_r15, CPU_r15
.endm

.irp op, shrd,shld
    .irp arg, imm,cl
        do_shiftd64 \op, \arg
    .endr
.endr

.irp op, shrd,shld
    .irp arg, imm,cl
        .gadget_array \op\()_\arg
    .endr
.endr

.macro do_bt_op op, arg, size, s
    .ifc \op,bt
        .ifnc \arg, w8
            mov w8, \arg
        .endif
        and _tmp, _tmp, \size-1
        lsr w8, w8, _tmp
        and w8, w8, 1
        strb w8, [_cpu, CPU_cf]
    .else
        mov w9, 1
        and _tmp, _tmp, \size-1
        lsl w9, w9, _tmp
        tst \arg, w9
        .ifc \op,btc
            eor \arg, \arg, w9
        .else N .ifc \op,bts
            orr \arg, \arg, w9
        .else N .ifc \op,btr
            bic \arg, \arg, w9
        .endif N .endif N .endif
        cset w9, ne
        strb w9, [_cpu, CPU_cf]
.endif
.endm

.macro do_bt_op64 op, arg
    .ifc \op,bt
        .ifnc \arg, x8
            mov x8, \arg
        .endif
        and _xtmp, _xtmp, 63
        lsr x8, x8, _xtmp
        and x8, x8, 1
        strb w8, [_cpu, CPU_cf]
    .else
        mov x9, 1
        and _xtmp, _xtmp, 63
        lsl x9, x9, _xtmp
        tst \arg, x9
        .ifc \op,btc
            eor \arg, \arg, x9
        .else N .ifc \op,bts
            orr \arg, \arg, x9
        .else N .ifc \op,btr
            bic \arg, \arg, x9
        .endif N .endif N .endif
        cset w9, ne
        strb w9, [_cpu, CPU_cf]
    .endif
.endm

.macro do_bt_high64 op, name, offset
    .gadget \op\()64_reg_\name
        ldr x8, [_cpu, \offset]
        do_bt_op64 \op, x8
        .ifin(\op, btc,bts,btr)
            str x8, [_cpu, \offset]
        .endifin
        gret
.endm

.macro do_bt_high op, size, s, name, offset
    .gadget \op\size\()_reg_\name
        ldr x8, [_cpu, \offset]
        .if \size == 16
            mov x11, x8
        .endif
        do_bt_op \op, w8, \size, \s
        .ifin(\op, btc,bts,btr)
            .if \size == 16
                bfi x11, x8, 0, 16
                str x11, [_cpu, \offset]
            .else
                uxtw x8, w8
                str x8, [_cpu, \offset]
            .endif
        .endifin
        gret
.endm

.macro do_bt64 op
    .gadget \op\()64_mem
        bic x8, _xtmp, 0x3f
        add _xaddr, _xaddr, x8, lsr 3
        .ifin(\op, bt)
            read_prep 64, \op\()64_mem
        .endifin
        .ifin(\op, btc,bts,btr)
            write_prep 64, \op\()64_mem
        .endifin
        ldr x8, [_xaddr]
        do_bt_op64 \op, x8
        .ifin(\op, btc,bts,btr)
            str x8, [_xaddr]
            write_done 64, \op\()64_mem
        .endifin
        gret 1
        .ifin(\op, bt)
            read_bullshit 64, \op\()64_mem
        .endifin
        .ifin(\op, btc,bts,btr)
            write_bullshit 64, \op\()64_mem
        .endifin

    .gadget \op\()64_reg_a
        do_bt_op64 \op, xax
        gret
    .gadget \op\()64_reg_b
        do_bt_op64 \op, xbx
        gret
    .gadget \op\()64_reg_c
        do_bt_op64 \op, xcx
        gret
    .gadget \op\()64_reg_d
        do_bt_op64 \op, xdx
        gret
    .gadget \op\()64_reg_si
        do_bt_op64 \op, xsi
        gret
    .gadget \op\()64_reg_di
        do_bt_op64 \op, xdi
        gret
    .gadget \op\()64_reg_bp
        do_bt_op64 \op, xbp
        gret
    .gadget \op\()64_reg_sp
        do_bt_op64 \op, xsp
        gret

    do_bt_high64 \op, r8, CPU_r8
    do_bt_high64 \op, r9, CPU_r9
    do_bt_high64 \op, r10, CPU_r10
    do_bt_high64 \op, r11, CPU_r11
    do_bt_high64 \op, r12, CPU_r12
    do_bt_high64 \op, r13, CPU_r13
    do_bt_high64 \op, r14, CPU_r14
    do_bt_high64 \op, r15, CPU_r15
.endm

.macro do_bt op, size, s
    .gadget \op\size\()_mem
        bic w8, _tmp, 0x1f
        add _xaddr, _xaddr, x8, lsr 3
        # hell {{{
        .ifin(\op, bt)
            read_prep \size, \op\size\()_mem
        .endifin
        .ifin(\op, btc,bts,btr)
            write_prep \size, \op\size\()_mem
        .endifin
        # }}}
        ldr w8, [_xaddr]
        do_bt_op \op, w8, \size, \s
        .ifin(\op, btc,bts,btr)
            str w8, [_xaddr]
            write_done \size, \op\size\()_mem
        .endifin
        gret 1
        # also hell {{{
        .ifin(\op, bt)
            read_bullshit \size, \op\size\()_mem
        .endifin
        .ifin(\op, btc,bts,btr)
            write_bullshit \size, \op\size\()_mem
        .endifin
        # }}}

    .macro x name, reg
        .gadget \op\size\()_\name
            do_bt_op \op, \reg, \size, \s
            gret
    .endm
    .each_reg x
    .purgem x

    do_bt_high \op, \size, \s, r8, CPU_r8
    do_bt_high \op, \size, \s, r9, CPU_r9
    do_bt_high \op, \size, \s, r10, CPU_r10
    do_bt_high \op, \size, \s, r11, CPU_r11
    do_bt_high \op, \size, \s, r12, CPU_r12
    do_bt_high \op, \size, \s, r13, CPU_r13
    do_bt_high \op, \size, \s, r14, CPU_r14
    do_bt_high \op, \size, \s, r15, CPU_r15
.endm

.irp op, bt,btc,bts,btr
    .irp size, 16,32
        ss \size, do_bt, \op
    .endr
    do_bt64 \op
    .gadget_array \op
.endr

# atomic versions of the above

.macro do_bt_atomic op, size, s
    .gadget atomic_\op\size\()_mem
        bic w8, _tmp, 0x1f
        add _xaddr, _xaddr, x8, lsr 3
        write_prep \size, atomic_\op\size\()_mem
        # this is simple enough that I'm comfortable doing it with ldaxr/stlxr
    1:
        ldaxr w8, [_xaddr]
        mov w9, 1
        and _tmp, _tmp, \size-1
        lsl w9, w9, _tmp
        tst w8, w9
        .ifc \op,btc
            eor w8, w8, w9
        .else N .ifc \op,bts
            orr w8, w8, w9
        .else N .ifc \op,btr
            bic w8, w8, w9
        .endif N .endif N .endif
        cset w9, ne
        stlxr w10, w8, [_xaddr]
        cbnz w10, 1b
        strb w9, [_cpu, CPU_cf]
        write_done \size, atomic_\op\size\()_mem
        gret 1
        write_bullshit \size, atomic_\op\size\()_mem
.endm

.irp op, btc,bts,btr
    .irp size, 16,32
        ss \size, do_bt_atomic, \op
    .endr
    .gadget_array atomic_\op
.endr

.macro x name reg
    .gadget bswap_\name
        rev \reg, \reg
        gret
.endm
.each_reg x
.purgem x

.macro bswap_high name, offset
    .gadget bswap_reg_\name
        ldr x8, [_cpu, \offset]
        rev w8, w8
        uxtw x8, w8
        str x8, [_cpu, \offset]
        gret
.endm
bswap_high r8, CPU_r8
bswap_high r9, CPU_r9
bswap_high r10, CPU_r10
bswap_high r11, CPU_r11
bswap_high r12, CPU_r12
bswap_high r13, CPU_r13
bswap_high r14, CPU_r14
bswap_high r15, CPU_r15
.purgem bswap_high
.gadget_list bswap, REG_LIST

.macro bswap64_low name, reg
    .gadget bswap64_reg_\name
        rev \reg, \reg
        gret
.endm
bswap64_low a, xax
bswap64_low c, xcx
bswap64_low d, xdx
bswap64_low b, xbx
bswap64_low sp, xsp
bswap64_low bp, xbp
bswap64_low si, xsi
bswap64_low di, xdi
.purgem bswap64_low

.macro bswap64_high name, offset
    .gadget bswap64_reg_\name
        ldr x8, [_cpu, \offset]
        rev x8, x8
        str x8, [_cpu, \offset]
        gret
.endm
bswap64_high r8, CPU_r8
bswap64_high r9, CPU_r9
bswap64_high r10, CPU_r10
bswap64_high r11, CPU_r11
bswap64_high r12, CPU_r12
bswap64_high r13, CPU_r13
bswap64_high r14, CPU_r14
bswap64_high r15, CPU_r15
.purgem bswap64_high
.gadget_list bswap64, REG_LIST
