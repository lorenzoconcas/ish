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

static void write_hex(unsigned char v) {
    static const char hex[] = "0123456789abcdef";
    char out[3] = { hex[v >> 4], hex[v & 15], ' ' };
    write_all(out, 3);
}

void _start(void) {
    unsigned long rsi = 0x1122334455667788ULL;
    unsigned long rdi = 0xaabbccddeeff0011ULL;
    unsigned long out_sil, out_dil, out_or;
    unsigned char sf_sil, zf_sil;

    __asm__ volatile(
        "mov %[rsi_in], %%rsi\n"
        "mov %[rdi_in], %%rdi\n"
        "movb $0x12, %%sil\n"
        "movb $0x34, %%dil\n"
        "mov %%rsi, %[out_sil]\n"
        "mov %%rdi, %[out_dil]\n"
        "orb %%dil, %%sil\n"
        "mov %%rsi, %[out_or]\n"
        "testb %%sil, %%sil\n"
        "sets %[sf]\n"
        "setz %[zf]\n"
        : [out_sil] "=r"(out_sil),
          [out_dil] "=r"(out_dil),
          [out_or] "=r"(out_or),
          [sf] "=qm"(sf_sil),
          [zf] "=qm"(zf_sil)
        : [rsi_in] "r"(rsi),
          [rdi_in] "r"(rdi)
        : "rsi", "rdi", "cc");

    write_all("sil=", 4); write_hex(out_sil & 0xff); write_hex((out_sil >> 8) & 0xff);
    write_all("dil=", 4); write_hex(out_dil & 0xff); write_hex((out_dil >> 8) & 0xff);
    write_all("or=", 3); write_hex(out_or & 0xff);
    write_all("sf=", 3); write_hex(sf_sil);
    write_all("zf=", 3); write_hex(zf_sil);
    write_all("\n", 1);
    syscall3(60, 0, 0, 0);
}
