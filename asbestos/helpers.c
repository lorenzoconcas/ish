#include <time.h>
#include "emu/cpu.h"
#include "emu/cpuid.h"

void helper_cpuid(dword_t *a, dword_t *b, dword_t *c, dword_t *d) {
    do_cpuid(a, b, c, d);
}

void helper_rdtsc(struct cpu_state *cpu) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    uint64_t tsc = now.tv_sec * 1000000000l + now.tv_nsec;
    cpu_set_compat_gpr(cpu, reg_eax, tsc & 0xffffffff);
    cpu_set_compat_gpr(cpu, reg_edx, tsc >> 32);
}

void helper_expand_flags(struct cpu_state *cpu) {
    expand_flags(cpu);
}

void helper_collapse_flags(struct cpu_state *cpu) {
    collapse_flags(cpu);
}

void helper_div64(struct cpu_state *cpu, qword_t divisor) {
    unsigned __int128 dividend = ((unsigned __int128) cpu->rdx << 64) | cpu->rax;
    cpu->rax = dividend / divisor;
    cpu->rdx = dividend % divisor;
}

void helper_idiv64(struct cpu_state *cpu, qword_t divisor) {
    __int128 dividend = ((__int128) (int64_t) cpu->rdx << 64) | cpu->rax;
    int64_t signed_divisor = divisor;
    cpu->rax = (qword_t) (dividend / signed_divisor);
    cpu->rdx = (qword_t) (dividend % signed_divisor);
}
