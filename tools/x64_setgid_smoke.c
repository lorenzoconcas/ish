static long syscall1(long n, long a) {
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a)
        : "rcx", "r11", "memory");
    return ret;
}

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

static void write_signed(long value) {
    char out[32];
    int pos = 0;
    if (value < 0) {
        out[pos++] = '-';
        value = -value;
    }
    char digits[20];
    int count = 0;
    do {
        digits[count++] = '0' + value % 10;
        value /= 10;
    } while (value != 0);
    while (count > 0)
        out[pos++] = digits[--count];
    out[pos++] = '\n';
    write_all(out, pos);
}

void _start(void) {
    write_signed(syscall1(106, 0));
    syscall3(60, 0, 0, 0);
}
