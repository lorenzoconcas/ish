#ifndef EMU_H
#define EMU_H

#include "misc.h"
#include "emu/mmu.h"
#include "emu/float80.h"

#ifdef __KERNEL__
#include <linux/stddef.h>
#else
#include <stddef.h>
#endif

struct cpu_state;
struct tlb;
int cpu_run_to_interrupt(struct cpu_state *cpu, struct tlb *tlb);
void cpu_poke(struct cpu_state *cpu);

union mm_reg {
    qword_t qw;
    dword_t dw[2];
};
union xmm_reg {
    unsigned __int128 u128;
    qword_t qw[2];
    uint32_t u32[4];
    uint16_t u16[8];
    uint8_t u8[16];
    float f32[4];
    double f64[2];
};
static_assert(sizeof(union xmm_reg) == 16, "xmm_reg size");
static_assert(sizeof(union mm_reg) == 8, "mm_reg size");

struct cpu_state {
    struct mmu *mmu;
    long cycle;

    // general registers
    // assumes little endian (as does literally everything)
#define _REG(n) \
    union { \
        dword_t e##n; \
        word_t n; \
    }
#define _REGX(n) \
    union { \
        dword_t e##n##x; \
        word_t n##x; \
        struct { \
            byte_t n##l; \
            byte_t n##h; \
        }; \
    }

    union {
        struct {
            _REGX(a);
            _REGX(c);
            _REGX(d);
            _REGX(b);
            _REG(sp);
            _REG(bp);
            _REG(si);
            _REG(di);
        };
        dword_t regs[8];
    };
#undef REGX
#undef REG

    // Staged long-mode register file. The existing execution engine still runs
    // on the compat registers above, so these must stay synchronized until the
    // decoder and gadgets switch over.
    union {
        struct {
            qword_t rax;
            qword_t rcx;
            qword_t rdx;
            qword_t rbx;
            qword_t rsp;
            qword_t rbp;
            qword_t rsi;
            qword_t rdi;
            qword_t r8;
            qword_t r9;
            qword_t r10;
            qword_t r11;
            qword_t r12;
            qword_t r13;
            qword_t r14;
            qword_t r15;
        };
        qword_t regs64[16];
    };

    dword_t eip;
    qword_t rip;

    // flags
    union {
        dword_t eflags;
        struct {
            bitfield cf_bit:1;
            bitfield pad1_1:1;
            bitfield pf:1;
            bitfield pad2_0:1;
            bitfield af:1;
            bitfield pad3_0:1;
            bitfield zf:1;
            bitfield sf:1;
            bitfield tf:1;
            bitfield if_:1;
            bitfield df:1;
            bitfield of_bit:1;
            bitfield iopl:2;
        };
        // for asm
#define PF_FLAG (1 << 2)
#define AF_FLAG (1 << 4)
#define ZF_FLAG (1 << 6)
#define SF_FLAG (1 << 7)
#define DF_FLAG (1 << 10)
    };
    // please pretend this doesn't exist
    dword_t df_offset;
    // for maximum efficiency these are stored in bytes
    byte_t cf;
    byte_t of;
    // whether the true flag values are in the above struct, or computed from
    // the stored result and operands
    qword_t res, op1, op2;
    union {
        struct {
            bitfield pf_res:1;
            bitfield zf_res:1;
            bitfield sf_res:1;
            bitfield af_ops:1;
        };
        // for asm
#define PF_RES (1 << 0)
#define ZF_RES (1 << 1)
#define SF_RES (1 << 2)
#define AF_OPS (1 << 3)
        byte_t flags_res;
    };

    union mm_reg mm[8];
    union xmm_reg xmm[16];

    // fpu
    float80 fp[8];
    union {
        word_t fsw;
        struct {
            bitfield ie:1; // invalid operation
            bitfield de:1; // denormalized operand
            bitfield ze:1; // divide by zero
            bitfield oe:1; // overflow
            bitfield ue:1; // underflow
            bitfield pe:1; // precision
            bitfield stf:1; // stack fault
            bitfield es:1; // exception status
            bitfield c0:1;
            bitfield c1:1;
            bitfield c2:1;
            unsigned top:3;
            bitfield c3:1;
            bitfield b:1; // fpu busy (?)
        };
    };
    union {
        word_t fcw;
        struct {
            bitfield im:1;
            bitfield dm:1;
            bitfield zm:1;
            bitfield om:1;
            bitfield um:1;
            bitfield pm:1;
            bitfield pad4:2;
            bitfield pc:2;
            bitfield rc:2;
            bitfield y:1;
        };
    };

    // TLS bullshit
    word_t gs;
    addr_t tls_ptr;
    addr_t fsbase;
    addr_t gsbase;

    // for the page fault handler
    addr_t segfault_addr;
    bool segfault_was_write;

    dword_t trapno;
    // access atomically
    bool *poked_ptr;
    bool _poked;
};

#define CPU_OFFSET(field) offsetof(struct cpu_state, field)

static_assert(CPU_OFFSET(eax) == CPU_OFFSET(regs[0]), "register order");
static_assert(CPU_OFFSET(ecx) == CPU_OFFSET(regs[1]), "register order");
static_assert(CPU_OFFSET(edx) == CPU_OFFSET(regs[2]), "register order");
static_assert(CPU_OFFSET(ebx) == CPU_OFFSET(regs[3]), "register order");
static_assert(CPU_OFFSET(esp) == CPU_OFFSET(regs[4]), "register order");
static_assert(CPU_OFFSET(ebp) == CPU_OFFSET(regs[5]), "register order");
static_assert(CPU_OFFSET(esi) == CPU_OFFSET(regs[6]), "register order");
static_assert(CPU_OFFSET(edi) == CPU_OFFSET(regs[7]), "register order");
static_assert(CPU_OFFSET(rax) == CPU_OFFSET(regs64[0]), "register order");
static_assert(CPU_OFFSET(rcx) == CPU_OFFSET(regs64[1]), "register order");
static_assert(CPU_OFFSET(rdx) == CPU_OFFSET(regs64[2]), "register order");
static_assert(CPU_OFFSET(rbx) == CPU_OFFSET(regs64[3]), "register order");
static_assert(CPU_OFFSET(rsp) == CPU_OFFSET(regs64[4]), "register order");
static_assert(CPU_OFFSET(rbp) == CPU_OFFSET(regs64[5]), "register order");
static_assert(CPU_OFFSET(rsi) == CPU_OFFSET(regs64[6]), "register order");
static_assert(CPU_OFFSET(rdi) == CPU_OFFSET(regs64[7]), "register order");
static_assert(sizeof(struct cpu_state) < 0xffff, "cpu struct is too big for vector gadgets");

// flags
#define ZF (cpu->zf_res ? cpu->res == 0 : cpu->zf)
#define SF (cpu->sf_res ? (int64_t) cpu->res < 0 : cpu->sf)
#define CF (cpu->cf)
#define OF (cpu->of)
#define PF (cpu->pf_res ? !__builtin_parity(cpu->res & 0xff) : cpu->pf)
#define AF (cpu->af_ops ? ((cpu->op1 ^ cpu->op2 ^ cpu->res) >> 4) & 1 : cpu->af)

static inline void collapse_flags(struct cpu_state *cpu) {
    cpu->zf = ZF;
    cpu->sf = SF;
    cpu->pf = PF;
    cpu->zf_res = cpu->sf_res = cpu->pf_res = 0;
    cpu->of_bit = cpu->of;
    cpu->cf_bit = cpu->cf;
    cpu->af = AF;
    cpu->af_ops = 0;
    cpu->pad1_1 = 1;
    cpu->pad2_0 = cpu->pad3_0 = 0;
    cpu->if_ = 1;
}

static inline void expand_flags(struct cpu_state *cpu) {
    cpu->of = cpu->of_bit;
    cpu->cf = cpu->cf_bit;
    cpu->zf_res = cpu->sf_res = cpu->pf_res = cpu->af_ops = 0;
}

enum reg32 {
    reg_eax = 0, reg_ecx, reg_edx, reg_ebx, reg_esp, reg_ebp, reg_esi, reg_edi, reg_count,
    reg_none = 16,
};

enum reg64 {
    reg_rax = 0, reg_rcx, reg_rdx, reg_rbx, reg_rsp, reg_rbp, reg_rsi, reg_rdi,
    reg_r8, reg_r9, reg_r10, reg_r11, reg_r12, reg_r13, reg_r14, reg_r15,
    reg64_count,
    reg64_none = reg64_count,
};

static inline const char *reg32_name(enum reg32 reg) {
    switch (reg) {
        case reg_eax: return "eax";
        case reg_ecx: return "ecx";
        case reg_edx: return "edx";
        case reg_ebx: return "ebx";
        case reg_esp: return "esp";
        case reg_ebp: return "ebp";
        case reg_esi: return "esi";
        case reg_edi: return "edi";
        default: return "?";
    }
}

static inline const char *reg64_name(enum reg64 reg) {
    switch (reg) {
        case reg_rax: return "rax";
        case reg_rcx: return "rcx";
        case reg_rdx: return "rdx";
        case reg_rbx: return "rbx";
        case reg_rsp: return "rsp";
        case reg_rbp: return "rbp";
        case reg_rsi: return "rsi";
        case reg_rdi: return "rdi";
        case reg_r8: return "r8";
        case reg_r9: return "r9";
        case reg_r10: return "r10";
        case reg_r11: return "r11";
        case reg_r12: return "r12";
        case reg_r13: return "r13";
        case reg_r14: return "r14";
        case reg_r15: return "r15";
        default: return "?";
    }
}

static inline enum reg64 reg32_to_reg64(enum reg32 reg) {
    assert(reg < reg_count);
    return (enum reg64) reg;
}

static inline dword_t cpu_compat_gpr(const struct cpu_state *cpu, enum reg32 reg) {
    assert(reg < reg_count);
    return cpu->regs[reg];
}

static inline void cpu_set_compat_gpr(struct cpu_state *cpu, enum reg32 reg, addr_t value) {
    assert(reg < reg_count);
    assert(value <= UINT32_MAX);
    cpu->regs[reg] = (dword_t) value;
    cpu->regs64[reg32_to_reg64(reg)] = value;
}

static inline qword_t cpu_gpr64(const struct cpu_state *cpu, enum reg64 reg) {
    assert(reg < reg64_count);
    return cpu->regs64[reg];
}

static inline void cpu_set_gpr64(struct cpu_state *cpu, enum reg64 reg, qword_t value) {
    assert(reg < reg64_count);
    cpu->regs64[reg] = value;
    if ((unsigned) reg < reg_count)
        cpu->regs[reg] = (dword_t) value;
}

static inline addr_t cpu_compat_ip(const struct cpu_state *cpu) {
    return cpu->eip;
}

static inline void cpu_set_compat_ip(struct cpu_state *cpu, addr_t ip) {
    assert(ip <= UINT32_MAX);
    cpu->eip = (dword_t) ip;
    cpu->rip = ip;
}

static inline qword_t cpu_ip64(const struct cpu_state *cpu) {
    return cpu->rip;
}

static inline void cpu_set_ip64(struct cpu_state *cpu, qword_t ip) {
    cpu->rip = ip;
    cpu->eip = (dword_t) ip;
}

static inline addr_t cpu_compat_sp(const struct cpu_state *cpu) {
    return cpu_compat_gpr(cpu, reg_esp);
}

static inline void cpu_set_compat_sp(struct cpu_state *cpu, addr_t sp) {
    cpu_set_compat_gpr(cpu, reg_esp, sp);
}

static inline void cpu_zero_compat_gprs(struct cpu_state *cpu) {
    for (size_t i = 0; i < reg_count; i++)
        cpu->regs[i] = 0;
    for (size_t i = 0; i < reg_count; i++)
        cpu->regs64[i] = 0;
}

static inline void cpu_zero_gprs64(struct cpu_state *cpu) {
    for (size_t i = 0; i < reg64_count; i++)
        cpu->regs64[i] = 0;
    for (size_t i = 0; i < reg_count; i++)
        cpu->regs[i] = 0;
}

static inline void cpu_sync_long_to_compat(struct cpu_state *cpu) {
    for (size_t i = 0; i < reg_count; i++)
        cpu->regs[i] = (dword_t) cpu->regs64[i];
    cpu->eip = (dword_t) cpu->rip;
}

static inline void cpu_sync_compat_to_long(struct cpu_state *cpu) {
    if (cpu->mmu != NULL && cpu->mmu->guest_word_size > sizeof(dword_t)) {
        cpu->eip = (dword_t) cpu->rip;
        return;
    }
    for (size_t i = 0; i < reg_count; i++)
        cpu->regs64[i] = cpu->regs[i];
    cpu->rip = cpu->eip;
}

#endif
