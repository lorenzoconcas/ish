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

static void print_u64(uint64_t v) {
    char c = (char)('0' + (v % 10));
    sys_write(1, &c, 1);
    sys_write(1, "\n", 1);
}

void _start(void) {
    uint64_t out0 = 0;
    uint64_t out1 = 0;

    __asm__ volatile (
        "xor %%eax, %%eax\n\t"
        "mov $0x94, %%r8b\n\t"
        "cmpb $0x9f, %%r8b\n\t"
        "ja 1f\n\t"
        "mov $0, %%eax\n\t"
        "jmp 2f\n\t"
        "1:\n\t"
        "mov $1, %%eax\n\t"
        "2:\n\t"
        "mov %%rax, %0\n\t"
        : "=r"(out0)
        :
        : "rax", "r8", "cc");

    __asm__ volatile (
        "xor %%eax, %%eax\n\t"
        "mov $0xa0, %%r8b\n\t"
        "cmpb $0x9f, %%r8b\n\t"
        "ja 1f\n\t"
        "mov $0, %%eax\n\t"
        "jmp 2f\n\t"
        "1:\n\t"
        "mov $1, %%eax\n\t"
        "2:\n\t"
        "mov %%rax, %0\n\t"
        : "=r"(out1)
        :
        : "rax", "r8", "cc");

    print_u64(out0);
    print_u64(out1);

    __asm__ volatile (
        "mov $60, %%rax\n\t"
        "xor %%rdi, %%rdi\n\t"
        "syscall\n\t"
        :
        :
        : "rax", "rdi", "rcx", "r11", "memory");
}
