typedef unsigned long u64;
typedef unsigned int u32;

#define AT_PHDR 3
#define AT_PHENT 4
#define AT_PHNUM 5
#define PT_TLS 7

__thread volatile char tlsbuf[0x158] __attribute__((used));

void *memcpy(void *dst, const void *src, u64 n) {
    char *d = dst;
    const char *s = src;
    while (n--)
        *d++ = *s++;
    return dst;
}

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

static void hex64(char *out, u64 v) {
    static const char h[] = "0123456789abcdef";
    for (int i = 15; i >= 0; i--) {
        out[i] = h[v & 0xf];
        v >>= 4;
    }
}

void start_c(u64 *sp) {
    u64 argc = *sp++;
    sp += argc;
    sp++;
    while (*sp)
        sp++;
    sp++;

    u64 phdr = 0;
    u64 phent = 0;
    u64 phnum = 0;
    for (; sp[0]; sp += 2) {
        if (sp[0] == AT_PHDR)
            phdr = sp[1];
        if (sp[0] == AT_PHENT)
            phent = sp[1];
        if (sp[0] == AT_PHNUM)
            phnum = sp[1];
    }

    if (phent != 56 || phnum == 0 || phdr == 0) {
        write_all("bad-aux\n", 8);
        exit_with(1);
    }

    for (u64 i = 0; i < phnum; i++) {
        unsigned char *p = (unsigned char *) (phdr + i * phent);
        u32 type = *(u32 *) (p + 0);
        if (type != PT_TLS)
            continue;
        u64 filesz = *(u64 *) (p + 32);
        u64 memsz = *(u64 *) (p + 40);
        u64 align = *(u64 *) (p + 48);
        if (filesz == 0 && memsz == 0x158 && align == 0x10) {
            write_all("ok\n", 3);
            exit_with(0);
        }
        char msg[] = "bad-tls filesz=0000000000000000 memsz=0000000000000000 align=0000000000000000\n";
        hex64(msg + 15, filesz);
        hex64(msg + 38, memsz);
        hex64(msg + 61, align);
        write_all(msg, sizeof(msg) - 1);
        exit_with(1);
    }

    write_all("missing-tls\n", 12);
    exit_with(1);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile("mov %rsp,%rdi; call start_c");
}
