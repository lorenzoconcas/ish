typedef unsigned long u64;

struct holder {
    char pad[0x10];
    void *cache;
};

struct holder obj;
char cache[0x180];
char src[] = "M";
char *src_ptr = src;
u64 qword_val = 0x12345678;

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

void _start(void) {
    obj.cache = cache;

    __asm__ volatile(
            "mov %[obj], %%rdi\n"
            "movq 0x10(%%rdi), %%rax\n"
            "test %%rax, %%rax\n"
            "je 1f\n"
            "movq %[qword], %%rdx\n"
            "leaq 2f(%%rip), %%rdi\n"
            "movq $0x0, 0x18(%%rax)\n"
            "movq %%rdi, 0x10(%%rax)\n"
            "movq %%rdx, 0x48(%%rax)\n"
            "movq %[src_slot], %%rdx\n"
            "movb $0x0, 0x20(%%rax)\n"
            "movq (%%rdx), %%rsi\n"
            "xorl %%edx, %%edx\n"
            "nop\n"
            "movsbl (%%rsi,%%rdx), %%ecx\n"
            "movl %%ecx, 0x50(%%rax,%%rdx,4)\n"
            "jmp 3f\n"
            "1:\n"
            "movq $0, 0x50\n"
            "2:\n"
            ".byte 0\n"
            "3:\n"
            :
            : [obj] "r"(&obj),
              [qword] "r"(qword_val),
              [src_slot] "r"(&src_ptr)
            : "rax", "rcx", "rdx", "rsi", "rdi", "memory");

    if (*(int *) (cache + 0x50) != 'M') {
        write_all("bad\n", 4);
        exit_with(1);
    }
    write_all("ok\n", 3);
    exit_with(0);
}
