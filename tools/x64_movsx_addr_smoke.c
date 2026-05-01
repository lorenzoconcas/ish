typedef unsigned long u64;

char src[] = "M";
u64 dst[32];

static long syscall3(long n, long a, long b, long c) {
    long ret;
    __asm__ volatile("syscall"
            : "=a"(ret)
            : "a"(n), "D"(a), "S"(b), "d"(c)
            : "rcx", "r11", "memory");
    return ret;
}

static long syscall6(long n, long a, long b, long c, long d, long e, long f) {
    long ret;
    register long r10 __asm__("r10") = d;
    register long r8 __asm__("r8") = e;
    register long r9 __asm__("r9") = f;
    __asm__ volatile("syscall"
            : "=a"(ret)
            : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8), "r"(r9)
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
    void *high = (void *) syscall6(9, 0, 0x1000, 3, 0x22, -1, 0);
    void *high_dst = (void *) syscall6(9, 0, 0x1000, 3, 0x22, -1, 0);
    if ((long) high < 0 || (long) high_dst < 0) {
        write_all("mmap\n", 5);
        exit_with(1);
    }
    *(char *) high = 'M';

    __asm__ volatile(
            "mov %1, %%rax\n"
            "mov %0, %%rsi\n"
            "xor %%edx, %%edx\n"
            "movsbl (%%rsi,%%rdx), %%ecx\n"
            "movl %%ecx, 0x50(%%rax,%%rdx,4)\n"
            :
            : "r"(high), "r"(high_dst)
            : "rax", "rcx", "rdx", "rsi", "memory");

    if (((unsigned char *) high_dst)[0x50] != 'M') {
        write_all("bad\n", 4);
        exit_with(1);
    }
    write_all("ok\n", 3);
    exit_with(0);
}
