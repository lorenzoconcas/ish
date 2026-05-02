typedef unsigned long u64;

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

static int fail;

static void check_u64(const char *name, u64 got, u64 want) {
    if (got != want) {
        write_all(name, 4);
        write_all("\n", 1);
        fail = 1;
    }
}

static u64 mem[16];

void _start(void) {
    u64 flags;

    mem[0] = 0xfffffffffffffffful;
    mem[1] = 0x123456789abcdef0ul;
    __asm__ volatile(
            "xor %%eax, %%eax\n"
            "cmp $1, %%rax\n"
            "adcq $0, %[m0]\n"
            "adcq $0x1234, %[m1]\n"
            "pushfq\n"
            "popq %[flags]\n"
            : [m0] "+m"(mem[0]), [m1] "+m"(mem[1]), [flags] "=r"(flags)
            :
            : "rax", "cc", "memory");
    check_u64("adcm", mem[0], 0);
    check_u64("adc2", mem[1], 0x123456789abcf125ul);
    check_u64("adcf", flags & 1, 0);

    mem[2] = 0;
    mem[3] = 0x1000;
    __asm__ volatile(
            "xor %%eax, %%eax\n"
            "cmp $1, %%rax\n"
            "sbbq $0, %[m2]\n"
            "sbbq $0x10, %[m3]\n"
            "pushfq\n"
            "popq %[flags]\n"
            : [m2] "+m"(mem[2]), [m3] "+m"(mem[3]), [flags] "=r"(flags)
            :
            : "rax", "cc", "memory");
    check_u64("sbbm", mem[2], 0xfffffffffffffffful);
    check_u64("sbb2", mem[3], 0xfef);
    check_u64("sbbf", flags & 1, 0);

    mem[4] = 0x0000000000000100ul;
    mem[5] = 0;
    __asm__ volatile(
            "addq $-0x100, %[m4]\n"
            "adcq $0, %[m5]\n"
            "pushfq\n"
            "popq %[flags]\n"
            : [m4] "+m"(mem[4]), [m5] "+m"(mem[5]), [flags] "=r"(flags)
            :
            : "cc", "memory");
    check_u64("addi", mem[4], 0);
    check_u64("addc", mem[5], 1);
    check_u64("addf", flags & 1, 0);

    mem[6] = 0x7777777777777777ul;
    mem[7] = 0x8888888888888888ul;
    u64 out = 0;
    __asm__ volatile(
            "mov $0, %%rax\n"
            "cmp $0, %%rax\n"
            "cmove %[m7], %[out]\n"
            : [out] "=r"(out)
            : [m7] "m"(mem[7])
            : "rax", "cc");
    check_u64("cmvm", out, 0x8888888888888888ul);

    if (fail)
        exit_with(1);
    write_all("ok\n", 3);
    exit_with(0);
}
