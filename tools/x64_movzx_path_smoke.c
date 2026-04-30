#include <stdint.h>

static long sys_write(int fd, const void *buf, unsigned long len) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "0"(1), "D"((long) fd), "S"(buf), "d"(len)
        : "rcx", "r11", "memory");
    return ret;
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
    uint64_t out0;
    uint64_t out1;
    uint64_t out2;

    __asm__ volatile (
        "mov $0x194, %%r8d\n\t"
        "mov %%r8d, %%eax\n\t"
        "mov %%rax, %0\n\t"
        : "=r"(out0)
        :
        : "rax", "r8");

    __asm__ volatile (
        "mov $0x194, %%r8d\n\t"
        "mov %%r8d, %%eax\n\t"
        "shrb $5, %%al\n\t"
        "mov %%rax, %0\n\t"
        : "=r"(out1)
        :
        : "rax", "r8");

    __asm__ volatile (
        "mov $0x194, %%r8d\n\t"
        "mov %%r8d, %%eax\n\t"
        "shrb $5, %%al\n\t"
        "movzbl %%al, %%eax\n\t"
        "mov %%rax, %0\n\t"
        : "=r"(out2)
        :
        : "rax", "r8");

    print_hex64(out0);
    print_hex64(out1);
    print_hex64(out2);

    __asm__ volatile (
        "mov $60, %%rax\n\t"
        "xor %%rdi, %%rdi\n\t"
        "syscall\n\t"
        :
        :
        : "rax", "rdi", "rcx", "r11", "memory");
}
