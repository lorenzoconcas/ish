extern int printf(const char *fmt, ...);

static void exit_syscall(int code) {
    __asm__ volatile(
        "mov $60, %%rax\n"
        "mov %0, %%rdi\n"
        "syscall\n"
        :
        : "r"((long) code)
        : "rax", "rdi", "rcx", "r11", "memory");
    __builtin_unreachable();
}

void _start(void) {
    printf("ptr=%p x=%02x d=%d ll=%llx str=%s\n",
           (void *) 0x12345678abcdef00ULL,
           0xab,
           12345,
           0x123456789abcdef0ULL,
           "ok");
    exit_syscall(0);
}
