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

void _start(void) {
    u64 lo, hi, flags;

    __asm__ volatile(
            "mov $0x123456789abcdef0, %%rax\n"
            "mov $0xfedcba9876543211, %%rcx\n"
            "mulq %%rcx\n"
            : "=a"(lo), "=d"(hi)
            :
            : "rcx", "cc");
    check_u64("mulL", lo, 0x35a1df76f0d5adf0ul);
    check_u64("mulH", hi, 0x121fa00ad77d7422ul);

    __asm__ volatile(
            "xor %%eax, %%eax\n"
            "cmp $1, %%rax\n"
            "mov $0xffffffffffffffff, %%rax\n"
            "adcq $0, %%rax\n"
            "pushfq\n"
            "popq %%rdx\n"
            : "=a"(lo), "=d"(flags)
            :
            : "cc");
    check_u64("adcL", lo, 0);
    check_u64("adcF", flags & 1, 1);

    __asm__ volatile(
            "xor %%eax, %%eax\n"
            "cmp $1, %%rax\n"
            "mov $0, %%rax\n"
            "sbbq $0, %%rax\n"
            "pushfq\n"
            "popq %%rdx\n"
            : "=a"(lo), "=d"(flags)
            :
            : "cc");
    check_u64("sbbL", lo, 0xfffffffffffffffful);
    check_u64("sbbF", flags & 1, 1);

    __asm__ volatile(
            "xor %%eax, %%eax\n"
            "cmp $1, %%rax\n"
            "mov $0x8000000000000000, %%r11\n"
            "adcq %%r11, %%r11\n"
            "mov %%r11, %%rax\n"
            "pushfq\n"
            "popq %%rdx\n"
            : "=a"(lo), "=d"(flags)
            :
            : "r11", "cc");
    check_u64("adcs", lo, 1);
    check_u64("adcf", flags & 1, 1);

    __asm__ volatile(
            "xor %%eax, %%eax\n"
            "cmp $1, %%rax\n"
            "mov $0, %%r11\n"
            "sbbq %%r11, %%r11\n"
            "mov %%r11, %%rax\n"
            "pushfq\n"
            "popq %%rdx\n"
            : "=a"(lo), "=d"(flags)
            :
            : "r11", "cc");
    check_u64("sbbs", lo, 0xfffffffffffffffful);
    check_u64("sbcf", flags & 1, 1);

    __asm__ volatile(
            "mov $0x0123456789abcdef, %%rax\n"
            "mov $0xfedcba9876543210, %%rdx\n"
            "shrdq $17, %%rdx, %%rax\n"
            : "=a"(lo)
            :
            : "rdx", "cc");
    check_u64("shrd", lo, 0x19080091a2b3c4d5ul);

    __asm__ volatile(
            "mov $0x0123456789abcdef, %%rax\n"
            "mov $0xfedcba9876543210, %%rdx\n"
            "shldq $17, %%rdx, %%rax\n"
            : "=a"(lo)
            :
            : "rdx", "cc");
    check_u64("shld", lo, 0x8acf13579bdffdb9ul);

    __asm__ volatile(
            "mov $7, %%rax\n"
            "mov $9, %%rdx\n"
            "cmp $7, %%rax\n"
            "cmove %%rdx, %%rax\n"
            : "=a"(lo)
            :
            : "rdx", "cc");
    check_u64("cmov", lo, 9);

    if (fail)
        exit_with(1);
    write_all("ok\n", 3);
    exit_with(0);
}
