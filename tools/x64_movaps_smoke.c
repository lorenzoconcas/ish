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

static u64 src[2] __attribute__((aligned(16))) = {
    0x0123456789abcdeful, 0xfedcba9876543210ul
};
static u64 mid[2] __attribute__((aligned(16)));
static u64 dst[2] __attribute__((aligned(16)));

void _start(void) {
    __asm__ volatile(
            "movaps %[src], %%xmm0\n"
            "movaps %%xmm0, %[mid]\n"
            "xorps %%xmm0, %%xmm0\n"
            "movaps %[mid], %%xmm1\n"
            "movaps %%xmm1, %[dst]\n"
            : [mid] "+m"(mid), [dst] "+m"(dst)
            : [src] "m"(src)
            : "xmm0", "xmm1", "memory");

    check_u64("mid0", mid[0], src[0]);
    check_u64("mid1", mid[1], src[1]);
    check_u64("dst0", dst[0], src[0]);
    check_u64("dst1", dst[1], src[1]);

    if (fail)
        exit_with(1);
    write_all("ok\n", 3);
    exit_with(0);
}
