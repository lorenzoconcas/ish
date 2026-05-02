#include <stdint.h>

static long syscall3(long n, long a, long b, long c) {
    long ret;
    __asm__ volatile("syscall"
            : "=a"(ret)
            : "a"(n), "D"(a), "S"(b), "d"(c)
            : "rcx", "r11", "memory");
    return ret;
}

static void write_all(const char *s, long len) {
    syscall3(1, 1, (long) s, len);
}

static void exit_with(long code) {
    syscall3(60, code, 0, 0);
    for (;;) {}
}

static uint32_t src[0x340];
static uint32_t dst[0x380];

static int fail;

static void check(const char *name, int ok) {
    if (!ok) {
        write_all(name, 4);
        write_all("\n", 1);
        fail = 1;
    }
}

void _start(void) {
    for (uint32_t i = 0; i < 0x340; i++)
        src[i] = 0x9e3779b9u * i + 0x1234567u;
    for (uint32_t i = 0; i < 0x380; i++)
        dst[i] = 0xa5a5a5a5u;

    uint64_t rcx = 0;
    uint64_t rsi = 0;
    uint64_t rdi = 0;
    __asm__ volatile(
            "lea %[src], %%rsi\n"
            "lea %[dst], %%rdi\n"
            "mov $0x300, %%rcx\n"
            "rep movsl\n"
            "mov %%rcx, %[rcx]\n"
            "mov %%rsi, %[rsi]\n"
            "mov %%rdi, %[rdi]\n"
            : [rcx] "=r"(rcx), [rsi] "=r"(rsi), [rdi] "=r"(rdi),
              [dst] "+m"(dst)
            : [src] "m"(src)
            : "rcx", "rsi", "rdi", "memory");

    check("rcxm", rcx == 0);
    check("rsim", rsi == (uint64_t) (src + 0x300));
    check("rdim", rdi == (uint64_t) (dst + 0x300));
    for (uint32_t i = 0; i < 0x300; i++)
        check("movs", dst[i] == src[i]);
    check("tail", dst[0x300] == 0xa5a5a5a5u);

    __asm__ volatile(
            "lea %[dst], %%rdi\n"
            "add $0x40, %%rdi\n"
            "xor %%eax, %%eax\n"
            "mov $0x80, %%rcx\n"
            "rep stosl\n"
            "mov %%rcx, %[rcx]\n"
            "mov %%rdi, %[rdi]\n"
            : [rcx] "=r"(rcx), [rdi] "=r"(rdi), [dst] "+m"(dst)
            :
            : "rax", "rcx", "rdi", "memory");

    check("rcxs", rcx == 0);
    check("rdis", rdi == (uint64_t) ((char *) dst + 0x40 + 0x80 * 4));
    for (uint32_t i = 0; i < 0x10; i++)
        check("pre ", dst[i] == src[i]);
    for (uint32_t i = 0x10; i < 0x90; i++)
        check("stos", dst[i] == 0);
    check("post", dst[0x90] == src[0x90]);

    if (fail)
        exit_with(1);
    write_all("ok\n", 3);
    exit_with(0);
}
