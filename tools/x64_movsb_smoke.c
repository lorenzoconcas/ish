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
    static const unsigned char source[] = "abcdef";
    unsigned char dest[8] = {0};
    unsigned long long rcx_out;
    unsigned long long rsi_out;
    unsigned long long rdi_out;

    __asm__ volatile(
        "lea %[src], %%rsi\n"
        "lea %[dst], %%rdi\n"
        "mov $6, %%rcx\n"
        "rep movsb\n"
        "mov %%rcx, %[rcx_out]\n"
        "mov %%rsi, %[rsi_out]\n"
        "mov %%rdi, %[rdi_out]\n"
        : [rcx_out] "=r"(rcx_out),
          [rsi_out] "=r"(rsi_out),
          [rdi_out] "=r"(rdi_out),
          [dst] "+m"(dest)
        : [src] "m"(source)
        : "rcx", "rsi", "rdi", "memory");

    write_all((const char *) dest, 6);
    write_all("\n", 1);
    write_hex(rcx_out);
    write_hex(rsi_out - (unsigned long long) source);
    write_hex(rdi_out - (unsigned long long) dest);
    syscall3(60, 0, 0, 0);
}