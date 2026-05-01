typedef unsigned long u64;

static u64 out[2] = {
    0xaaaaaaaaaaaaaaaaull,
    0xbbbbbbbbbbbbbbbbull,
};
static u64 movhl;

static long syscall3(long n, long a, long b, long c) {
    long ret;
    __asm__ volatile("syscall"
            : "=a"(ret)
            : "a"(n), "D"(a), "S"(b), "d"(c)
            : "rcx", "r11", "memory");
    return ret;
}

static void write_all(const char *s, u64 n) {
    syscall3(1, 1, (long) s, n);
}

static void exit_with(long code) {
    syscall3(60, code, 0, 0);
    for (;;) {}
}

void _start(void) {
    __asm__ volatile(
            "mov $0x150, %%rax\n"
            "mov $0x8, %%rdi\n"
            "movq %%rax, %%xmm1\n"
            "movq %%rdi, %%xmm0\n"
            "punpcklqdq %%xmm1, %%xmm0\n"
            "movups %%xmm0, out(%%rip)\n"
            "movdqu out(%%rip), %%xmm0\n"
            "movhlps %%xmm0, %%xmm2\n"
            "movq %%xmm2, movhl(%%rip)\n"
            :
            :
            : "rax", "rdi", "xmm0", "xmm1", "xmm2", "memory");

    if (out[0] != 0x8 || out[1] != 0x150 || movhl != 0x150) {
        write_all("bad\n", 4);
        exit_with(1);
    }
    write_all("ok\n", 3);
    exit_with(0);
}
