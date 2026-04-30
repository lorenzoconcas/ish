static long syscall3(long n, long a, long b, long c) {
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a), "S"(b), "d"(c)
        : "rcx", "r11", "memory");
    return ret;
}

static void write_all(const char *s, long len) {
    syscall3(1, 1, (long) s, len);
}

static void write_hex(unsigned long long value) {
    static const char hex[] = "0123456789abcdef";
    char out[17];
    for (int index = 15; index >= 0; index--) {
        out[index] = hex[value & 0xf];
        value >>= 4;
    }
    out[16] = '\n';
    write_all(out, 17);
}

void _start(void) {
    unsigned long long out_r9;
    unsigned long long out_rbp;

    __asm__ volatile(
        "mov $0x1122334455667788, %%r10\n"
        ".byte 0x66, 0x49, 0x0f, 0x6e, 0xda\n"
        "mov $0xaabbccddeeff0011, %%r9\n"
        ".byte 0x66, 0x49, 0x0f, 0x7e, 0xd9\n"
        "mov %%r9, %[out_r9]\n"
        "mov $0x99aabbccddeeff00, %%rcx\n"
        ".byte 0x66, 0x48, 0x0f, 0x6e, 0xd1\n"
        "mov $0x123456789abcdef0, %%rbp\n"
        ".byte 0x66, 0x48, 0x0f, 0x7e, 0xd5\n"
        "mov %%rbp, %[out_rbp]\n"
        : [out_r9] "=r"(out_r9),
          [out_rbp] "=r"(out_rbp)
        :
        : "rcx", "rbp", "r9", "r10", "xmm2", "xmm3", "cc");

    write_hex(out_r9);
    write_hex(out_rbp);
    syscall3(60, 0, 0, 0);
}
