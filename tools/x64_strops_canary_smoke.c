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

static void write_all(const char *s, long len) {
    syscall3(1, 1, (long) s, len);
}

static void write_hex(uint64_t value) {
    static const char hex[] = "0123456789abcdef";
    char out[17];
    for (int i = 15; i >= 0; --i) {
        out[i] = hex[value & 0xf];
        value >>= 4;
    }
    out[16] = '\n';
    write_all(out, 17);
}

void _start(void) {
    static unsigned char src[0x900];
    static unsigned char dst[0xa00];

    for (unsigned i = 0; i < sizeof(src); i++)
        src[i] = (unsigned char) (i * 17 + 3);
    for (unsigned i = 0; i < sizeof(dst); i++)
        dst[i] = 0xaa;

    uint64_t rcx_movs;
    uint64_t rsi_delta;
    uint64_t rdi_delta;
    __asm__ volatile(
        "lea %[src], %%rsi\n\t"
        "lea %[dst], %%rdi\n\t"
        "mov $0x861, %%rcx\n\t"
        "rep movsb\n\t"
        "mov %%rcx, %[rcx]\n\t"
        "mov %%rsi, %[rsi]\n\t"
        "mov %%rdi, %[rdi]\n\t"
        : [rcx] "=r"(rcx_movs),
          [rsi] "=r"(rsi_delta),
          [rdi] "=r"(rdi_delta),
          [dst] "+m"(dst)
        : [src] "m"(src)
        : "rcx", "rsi", "rdi", "memory");
    rsi_delta -= (uint64_t) src;
    rdi_delta -= (uint64_t) dst;

    uint64_t bad = 0;
    for (unsigned i = 0; i < 0x861; i++)
        if (dst[i] != src[i])
            bad++;
    for (unsigned i = 0x861; i < sizeof(dst); i++)
        if (dst[i] != 0xaa)
            bad++;

    __asm__ volatile(
        "lea %[dst], %%rdi\n\t"
        "add $0x100, %%rdi\n\t"
        "xor %%eax, %%eax\n\t"
        "mov $0x194, %%rcx\n\t"
        "rep stosb\n\t"
        "mov %%rcx, %[rcx]\n\t"
        : [rcx] "=r"(rcx_movs),
          [dst] "+m"(dst)
        :
        : "rax", "rcx", "rdi", "memory");

    for (unsigned i = 0x100; i < 0x294; i++)
        if (dst[i] != 0)
            bad++;
    if (dst[0xff] != src[0xff] || dst[0x294] != src[0x294])
        bad++;

    write_hex(bad);
    write_hex(rcx_movs);
    write_hex(rsi_delta);
    write_hex(rdi_delta);
    syscall3(60, 0, 0, 0);
}
