#include <stdint.h>

static unsigned char buf[64];

static long sys_write(int fd, const void *p, unsigned long n) {
    long r;
    __asm__ volatile (
        "syscall"
        : "=a"(r)
        : "0"(1), "D"((long) fd), "S"(p), "d"(n)
        : "rcx", "r11", "memory");
    return r;
}

static void print_hex64(uint64_t x) {
    char out[17];
    static const char h[] = "0123456789abcdef";
    for (int i = 15; i >= 0; --i) {
        out[i] = h[x & 0xf];
        x >>= 4;
    }
    sys_write(1, out, 16);
    sys_write(1, "\n", 1);
}

void _start(void) {
    for (int i = 0; i < 64; i++)
        buf[i] = 0;

    uint64_t out0, out1, out2, out3;
    uint64_t loaded;
    __asm__ volatile (
        "leaq buf(%%rip), %%r9\n\t"
        "addq $32, %%r9\n\t"
        "movb $0xaa, -5(%%r9)\n\t"
        "movb $0xff, -3(%%r9)\n\t"
        "movw $0x1234, -2(%%r9)\n\t"
        "movl -4(%%r9), %%eax\n\t"
        "mov %%rax, %0\n\t"
        : "=r"(loaded)
        :
        : "rax", "r9", "memory");

    out0 = buf[27];
    out1 = buf[29];
    out2 = buf[30] | ((uint64_t) buf[31] << 8);
    out3 = loaded;

    print_hex64(out0);
    print_hex64(out1);
    print_hex64(out2);
    print_hex64(out3);

    __asm__ volatile (
        "mov $60, %%rax\n\t"
        "xor %%rdi, %%rdi\n\t"
        "syscall\n\t"
        :
        :
        : "rax", "rdi", "rcx", "r11", "memory");
}
