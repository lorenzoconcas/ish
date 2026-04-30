extern int EVP_DecodeBlock(unsigned char *out, const unsigned char *in, int inlen);

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

static void write_dec(int value) {
    char buf[16];
    int pos = sizeof(buf);
    if (value == 0) {
        write_all("0", 1);
        return;
    }
    while (value > 0 && pos > 0) {
        buf[--pos] = '0' + value % 10;
        value /= 10;
    }
    write_all(&buf[pos], sizeof(buf) - pos);
}

static void run_case(const char *label, const unsigned char *in, int len) {
    unsigned char out[64] = {0};
    int ret = EVP_DecodeBlock(out, in, len);
    write_all(label, 2);
    write_all(" ret=", 5);
    write_dec(ret);
    write_all(" data=", 6);
    for (int i = 0; i < ret && i < 16; i++) write_hex(out[i]);
    write_all("\n", 1);
}

void _start(void) {
    static const unsigned char a[] = "TWFu";
    static const unsigned char b[] = "TWFuTWFu";
    static const unsigned char c[] = "TWFuTWFuTWFu";
    run_case("a4", a, 4);
    run_case("b8", b, 8);
    run_case("c1", c, 12);
    syscall3(60, 0, 0, 0);
}
