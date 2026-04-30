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
    unsigned long long value = 0x0123456789abcdefULL;
    unsigned char cf_bt;
    unsigned char cf_bts;
    unsigned char cf_btr;
    unsigned char cf_btc;

    __asm__ volatile(
        "btq $7, %[val]\n"
        "setc %[cf_bt]\n"
        "btsq $7, %[val]\n"
        "setc %[cf_bts]\n"
        "btrq $0, %[val]\n"
        "setc %[cf_btr]\n"
        "btcq $4, %[val]\n"
        "setc %[cf_btc]\n"
        : [val] "+r"(value),
          [cf_bt] "=qm"(cf_bt),
          [cf_bts] "=qm"(cf_bts),
          [cf_btr] "=qm"(cf_btr),
          [cf_btc] "=qm"(cf_btc)
        :
        : "cc");

    write_hex(value);
    write_hex(cf_bt);
    write_hex(cf_bts);
    write_hex(cf_btr);
    write_hex(cf_btc);
    syscall3(60, 0, 0, 0);
}