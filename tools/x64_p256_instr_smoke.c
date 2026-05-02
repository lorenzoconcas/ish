typedef unsigned long u64;
typedef unsigned int u32;

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

static int fail;

static void check_u64(const char *name, u64 got, u64 want) {
    if (got != want) {
        write_all(name, 4);
        write_all("\n", 1);
        fail = 1;
    }
}

static unsigned char bits[4] = {0x80, 0x01, 0x7f, 0xff};
static unsigned char byte_out;

void _start(void) {
    u64 rax, rdx, flags;

    __asm__ volatile(
            "mov $0x8000000000000001, %%rax\n"
            "btrq $0x3f, %%rax\n"
            "pushfq\n"
            "popq %%rdx\n"
            : "=a"(rax), "=d"(flags)
            :
            : "cc");
    check_u64("btrv", rax, 1);
    check_u64("btrc", flags & 1, 1);

    __asm__ volatile(
            "mov $0x8000000000000000, %%rax\n"
            "negq %%rax\n"
            "pushfq\n"
            "popq %%rdx\n"
            : "=a"(rax), "=d"(flags)
            :
            : "cc");
    check_u64("negv", rax, 0x8000000000000000ul);
    check_u64("nego", (flags >> 11) & 1, 1);
    check_u64("negc", flags & 1, 1);

    __asm__ volatile(
            "mov $0xffffffff, %%eax\n"
            "cltd\n"
            "mov %%edx, %%ecx\n"
            "mov $5, %%edi\n"
            "idivl %%edi\n"
            : "=a"(rax), "=c"(rdx)
            :
            : "rdx", "rdi", "cc");
    check_u64("idiv", (u32) rax, 0);
    check_u64("irem", (u32) rdx, 0xffffffffu);

    __asm__ volatile(
            "mov $0, %%eax\n"
            "cmp $1, %%eax\n"
            "sbbl %%ebx, %%ebx\n"
            "andb $0x20, %%bl\n"
            "addl $0xff, %%ebx\n"
            "mov %%ebx, %%eax\n"
            : "=a"(rax)
            :
            : "rbx", "cc");
    check_u64("sbbl", (u32) rax, 0x1f);

    __asm__ volatile(
            "mov $0, %%eax\n"
            "test %%rax, %%rax\n"
            "setne %[out]\n"
            : [out] "=m"(byte_out)
            :
            : "rax", "cc", "memory");
    check_u64("set0", byte_out, 0);

    __asm__ volatile(
            "mov $0xff, %%eax\n"
            "movsbl %%al, %%ecx\n"
            "movsbq %%al, %%rdx\n"
            : "=c"(rax), "=d"(rdx)
            :
            : "rax");
    check_u64("msbl", (u32) rax, 0xffffffffu);
    check_u64("msbq", rdx, 0xfffffffffffffffful);

    __asm__ volatile(
            "mov $15, %%esi\n"
            "mov %%esi, %%eax\n"
            "andl $7, %%esi\n"
            "sarl $3, %%eax\n"
            "cltq\n"
            "mov %[bits], %%rdi\n"
            "movzbl (%%rdi,%%rax), %%eax\n"
            "btl %%esi, %%eax\n"
            "setb %%al\n"
            : "=a"(rax)
            : [bits] "b"(bits)
            : "rdi", "rsi", "cc");
    check_u64("bitx", rax & 0xff, 0);

    if (fail)
        exit_with(1);
    write_all("ok\n", 3);
    exit_with(0);
}
