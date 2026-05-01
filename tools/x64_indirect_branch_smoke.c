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

static void call_target(void) {
    __asm__ volatile("ret");
}

static void jmp_target(void) {
    write_all("ok\n", 3);
    exit_with(0);
}

void _start(void) {
    void (*call_ptr)(void) = call_target;
    void (*jmp_ptr)(void) = jmp_target;

    __asm__ volatile(
            "mov %[call_ptr], %%rax\n"
            ".byte 0x66\n"
            "call *%%rax\n"
            "mov %[jmp_ptr], %%rax\n"
            ".byte 0x66\n"
            "jmp *%%rax\n"
            :
            : [call_ptr] "r"(call_ptr),
              [jmp_ptr] "r"(jmp_ptr)
            : "rax", "memory");

    write_all("bad\n", 4);
    exit_with(1);
}
