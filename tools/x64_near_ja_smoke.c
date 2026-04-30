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

static void print_digit(uint64_t x) {
    char c = '0' + (char) x;
    sys_write(1, &c, 1);
    sys_write(1, "\n", 1);
}

void _start(void) {
    uint64_t below;
    uint64_t above;

    __asm__ volatile (
        "xor %%eax, %%eax\n\t"
        "mov $0x94, %%r8b\n\t"
        "cmpb $0x9f, %%r8b\n\t"
        ".byte 0x0f, 0x87\n\t"
        ".long 1f - . - 4\n\t"
        "mov $0, %%eax\n\t"
        "jmp 2f\n\t"
        "1: mov $1, %%eax\n\t"
        "2: mov %%rax, %0\n\t"
        : "=r"(below)
        :
        : "rax", "r8", "cc");

    __asm__ volatile (
        "xor %%eax, %%eax\n\t"
        "mov $0xa4, %%r8b\n\t"
        "cmpb $0x9f, %%r8b\n\t"
        ".byte 0x0f, 0x87\n\t"
        ".long 1f - . - 4\n\t"
        "mov $0, %%eax\n\t"
        "jmp 2f\n\t"
        "1: mov $1, %%eax\n\t"
        "2: mov %%rax, %0\n\t"
        : "=r"(above)
        :
        : "rax", "r8", "cc");

    print_digit(below);
    print_digit(above);

    __asm__ volatile (
        "mov $60, %%rax\n\t"
        "xor %%rdi, %%rdi\n\t"
        "syscall\n\t"
        :
        :
        : "rax", "rdi", "rcx", "r11", "memory");
}
