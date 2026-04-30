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

    uint64_t c0, c1, loaded;
    __asm__ volatile (
        "leaq buf(%%rip), %%rdi\n\t"
        "addq $32, %%rdi\n\t"
        "movb $0x00, -5(%%rdi)\n\t"
        "movl $0x00000194, -4(%%rdi)\n\t"
        "movl -4(%%rdi), %%eax\n\t"
        "cmpb $0x0, -5(%%rdi)\n\t"
        "mov %%rax, %0\n\t"
        "setne %%al\n\t"
        "movzx %%al, %%eax\n\t"
        "mov %%rax, %1\n\t"
        : "=r"(loaded), "=r"(c1)
        :
        : "rax", "rdi", "cc", "memory");

    c0 = buf[27];
    print_hex64(c0);
    print_hex64(loaded);
    print_hex64(c1);

    __asm__ volatile (
        "mov $60, %%rax\n\t"
        "xor %%rdi, %%rdi\n\t"
        "syscall\n\t"
        :
        :
        : "rax", "rdi", "rcx", "r11", "memory");
}
