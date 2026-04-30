static unsigned char zero_page[4096];

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

static unsigned long long count_nonzero(const unsigned char *buffer, unsigned long long size) {
    unsigned long long count = 0;
    for (unsigned long long index = 0; index < size; index++)
        if (buffer[index] != 0)
            count++;
    return count;
}

void _start(void) {
    unsigned long long bss_nonzero = count_nonzero(zero_page, sizeof(zero_page));
    unsigned long long current_brk = syscall3(12, 0, 0, 0);
    unsigned long long new_brk = current_brk + 8192;
    unsigned long long result_brk = syscall3(12, new_brk, 0, 0);
    unsigned long long brk_nonzero = 0;

    if (result_brk == new_brk)
        brk_nonzero = count_nonzero((const unsigned char *) current_brk, 8192);

    write_hex(bss_nonzero);
    write_hex(result_brk);
    write_hex(brk_nonzero);
    syscall3(60, 0, 0, 0);
}