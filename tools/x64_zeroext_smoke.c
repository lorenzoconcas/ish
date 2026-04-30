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
    unsigned long long rax_out;
    unsigned long long rdi_out;

    __asm__ volatile(
        "mov $0xffffffffffffffff, %%rax\n"
        "mov $0xffffffffffffffff, %%rdi\n"
        "mov $0x12345678, %%r8d\n"
        "mov %%r8d, %%eax\n"
        "mov $0xab, %%cl\n"
        "movzbl %%cl, %%edi\n"
        "mov %%rax, %[rax_out]\n"
        "mov %%rdi, %[rdi_out]\n"
        : [rax_out] "=r"(rax_out),
          [rdi_out] "=r"(rdi_out)
        :
        : "rax", "rcx", "rdi", "r8", "cc");

    write_hex(rax_out);
    write_hex(rdi_out);
    syscall3(60, 0, 0, 0);
}