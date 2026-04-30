#include "gadgets.h"
#include "math.h"

#define REP_LIST once,rep,repnz

.gadget cld
    ldr w8, [_cpu, CPU_eflags]
    bic w8, w8, DF_FLAG
    str w8, [_cpu, CPU_eflags]
    gret

.gadget std
    ldr w8, [_cpu, CPU_eflags]
    orr w8, w8, DF_FLAG
    str w8, [_cpu, CPU_eflags]
    gret

.macro do_strop op, size, rep, name, addr64, s=
    # repnz is only a thing for cmps and scas
    .ifc \rep,repnz
        .ifnc \op,cmps N .ifnc \op,scas
            .exitm
        .endif N .endif
    .endif

    .gadget \name\size\()_\rep
        .ifnc \rep,once
            .if \addr64
                cbz xcx, 2f
            .else
                cbz ecx, 2f
            .endif
        .endif
        # df_offset = w12/x12
        .if \addr64
            mov x12, (\size/8)
        .else
            mov w12, (\size/8)
        .endif
        ldr w8, [_cpu, CPU_eflags]
        tst w8, DF_FLAG
        .if \addr64
            cneg x12, x12, ne
        .else
            cneg w12, w12, ne
        .endif
    1:

        .ifc \op,lods
            .if \addr64
                mov _xaddr, xsi
            .else
                mov _addr, esi
            .endif
            read_prep \size, \name\size\()_\rep
            .if \size == 64
                ldr xax, [_xaddr]
            .else
                ldrs eax, [_xaddr], \s
            .endif

        .else N .ifc \op,stos
            .if \addr64
                mov _xaddr, xdi
            .else
                mov _addr, edi
            .endif
            write_prep \size, \name\size\()_\rep
            .if \size == 64
                str xax, [_xaddr]
            .else
                str\s eax, [_xaddr]
            .endif
            write_done \size, \name\size\()_\rep

        .else N .ifc \op,movs
            .if \addr64
                mov _xaddr, xsi
            .else
                mov _addr, esi
            .endif
            read_prep \size, \name\size\()_\rep
            .if \size == 64
                ldr _xtmp, [_xaddr]
            .else
                ldr\s _tmp, [_xaddr]
            .endif
            .if \addr64
                mov _xaddr, xdi
            .else
                mov _addr, edi
            .endif
            write_prep \size, \name\size\()_\rep\()2
            .if \size == 64
                str _xtmp, [_xaddr]
            .else
                str\s _tmp, [_xaddr]
            .endif
            write_done \size, \name\size\()_\rep\()2

        .else N .ifc \op, scas
            .if \addr64
                mov _xaddr, xdi
            .else
                mov _addr, edi
            .endif
            read_prep \size, \name\size\()_\rep
            .if \size == 64
                ldr x8, [_xaddr]
                mov _xtmp, xax
                setf_a64 x8, _xtmp
                subs _xtmp, _xtmp, x8
                cset w10, vs
                strb w10, [_cpu, CPU_of]
                cset w10, cc
                strb w10, [_cpu, CPU_cf]
                setf_zsp x, val=_xtmp
            .else
                ldr\s w8, [_xaddr]
                mov _tmp, eax
                setf_a src=w8, dst=_tmp
                do_add sub, _tmp, w8, \s
                setf_zsp \s
            .endif

        .else N .ifc \op, cmps
            .if \addr64
                mov _xaddr, xsi
            .else
                mov _addr, esi
            .endif
            read_prep \size, \name\size\()_\rep
            .if \size == 64
                ldr _xtmp, [_xaddr]
            .else
                ldr\s _tmp, [_xaddr]
            .endif
            .if \addr64
                mov _xaddr, xdi
            .else
                mov _addr, edi
            .endif
            read_prep \size, \name\size\()_\rep\()2
            .if \size == 64
                ldr x8, [_xaddr]
                setf_a64 x8, _xtmp
                subs _xtmp, _xtmp, x8
                cset w10, vs
                strb w10, [_cpu, CPU_of]
                cset w10, cc
                strb w10, [_cpu, CPU_cf]
                setf_zsp x, val=_xtmp
            .else
                ldr\s w8, [_xaddr]
                setf_a src=w8, dst=_tmp
                do_add sub, _tmp, w8, \s
                setf_zsp \s
            .endif
        .endif N .endif N .endif N .endif N .endif

        .ifin(\op, lods,movs,cmps)
            .if \addr64
                add xsi, xsi, x12
            .else
                add esi, esi, w12
            .endif
        .endifin
        .ifin(\op, movs,stos,cmps,scas)
            .if \addr64
                add xdi, xdi, x12
            .else
                add edi, edi, w12
            .endif
        .endifin

        .ifnc \rep,once
            .if \addr64
                subs xcx, xcx, 1
            .else
                subs ecx, ecx, 1
            .endif
            .ifin(\op, scas,cmps)
                .ifc \rep,rep
                    .if \size == 64
                        cbnz _xtmp, 2f
                    .else
                        cbnz _tmp, 2f
                    .endif
                .else N .ifc \rep,repnz
                    .if \size == 64
                        cbz _xtmp, 2f
                    .else
                        cbz _tmp, 2f
                    .endif
                .endif N .endif
            .endifin
            .if \addr64
                cbnz xcx, 1b
            .else
                cbnz ecx, 1b
            .endif
    2:
        .endif
        gret 1
        .ifin(\op, lods,movs,scas,cmps)
            read_bullshit \size, \name\size\()_\rep
        .endifin
        .ifc \op,stos
            write_bullshit \size, \name\size\()_\rep
        .endif
        .ifc \op,movs
            write_bullshit \size, \name\size\()_\rep\()2
        .endif
        .ifc \op,cmps
            read_bullshit \size, \name\size\()_\rep\()2
        .endif
.endm

.irp op, lods,stos,movs,scas,cmps
    .irp size, 8,16,32,64
        .irp rep, REP_LIST
            .if \size == 8
                do_strop \op, \size, \rep, \op, 0, b
                do_strop \op, \size, \rep, \op\()64addr, 1, b
            .elseif \size == 16
                do_strop \op, \size, \rep, \op, 0, h
                do_strop \op, \size, \rep, \op\()64addr, 1, h
            .elseif \size == 32
                do_strop \op, \size, \rep, \op, 0
                do_strop \op, \size, \rep, \op\()64addr, 1
            .elseif \size == 64
                do_strop \op, \size, \rep, \op, 0, x
                do_strop \op, \size, \rep, \op\()64addr, 1, x
            .endif
        .endr
    .endr
    .gadget_list_size \op, REP_LIST
    .gadget_list_size \op\()64addr, REP_LIST
.endr
# temporary
