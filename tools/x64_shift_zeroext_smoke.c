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
    unsigned long long out_addr32;
    unsigned long long out_plain;
    unsigned long long out_path;
    unsigned long long out_lea;
    unsigned long long expect_lea;

    __asm__ volatile(
        "mov $0x71415ee800000040, %%r9\n"
        ".byte 0x67, 0x41, 0xc1, 0xe1, 0x03\n"
        "mov %%r9, %[out_addr32]\n"
        "mov $0x71415ee800000040, %%r9\n"
        ".byte 0x41, 0xc1, 0xe1, 0x03\n"
        "mov %%r9, %[out_plain]\n"
        "mov $0x71415ee800000040, %%r9\n"
        ".byte 0x41, 0xc1, 0xe1, 0x03\n"
        "neg %%r9\n"
        "neg %%r9\n"
        "shl $5, %%r9\n"
        "shr $5, %%r9\n"
        "neg %%r9\n"
        "mov %%r9, %[out_path]\n"
        "mov %%rsp, %%rax\n"
        "add $0x38, %%rax\n"
        "mov %%rax, %[expect_lea]\n"
        "mov $0x71415ee800000200, %%r9\n"
        "lea 0x38(%%rsp,%%r9), %%rdi\n"
        "neg %%r9\n"
        "lea (%%rdi,%%r9), %%rdi\n"
        "mov %%rdi, %[out_lea]\n"
        : [out_addr32] "=r"(out_addr32),
          [out_plain] "=r"(out_plain),
          [out_path] "=r"(out_path),
          [out_lea] "=r"(out_lea),
          [expect_lea] "=r"(expect_lea)
        :
        : "rax", "rdi", "r9", "cc");

    write_hex(out_addr32);
    write_hex(out_plain);
    write_hex(out_path);
    write_hex(expect_lea);
    write_hex(out_lea);
    syscall3(60, 0, 0, 0);
}
