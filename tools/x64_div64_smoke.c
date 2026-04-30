#include <stdint.h>

static long syscall3(long n, long a, long b, long c) {
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a), "S"(b), "d"(c)
        : "rcx", "r11", "memory");
    return ret;
}

static void write_hex(uint64_t value) {
    static const char hex[] = "0123456789abcdef";
    char out[17];
    for (int i = 15; i >= 0; --i) {
        out[i] = hex[value & 0xf];
        value >>= 4;
    }
    out[16] = '\n';
    syscall3(1, 1, (long) out, 17);
}

void _start(void) {
    uint64_t q;
    uint64_t r;

    __asm__ volatile(
        "mov $0x899, %%rax\n\t"
        "xor %%rdx, %%rdx\n\t"
        "mov $3, %%rcx\n\t"
        "divq %%rcx\n\t"
        "mov %%rax, %0\n\t"
        "mov %%rdx, %1\n\t"
        : "=r"(q), "=r"(r)
        :
        : "rax", "rcx", "rdx", "cc");

    write_hex(q);
    write_hex(r);

    __asm__ volatile(
        "mov $0x123456789abcdef0, %%rax\n\t"
        "mov $0x2, %%rdx\n\t"
        "mov $0x12345, %%rcx\n\t"
        "divq %%rcx\n\t"
        "mov %%rax, %0\n\t"
        "mov %%rdx, %1\n\t"
        : "=r"(q), "=r"(r)
        :
        : "rax", "rcx", "rdx", "cc");

    write_hex(q);
    write_hex(r);
    syscall3(60, 0, 0, 0);
}
