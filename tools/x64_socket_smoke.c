#include <stdint.h>

typedef unsigned long u64;
typedef long i64;

static i64 syscall6(i64 n, i64 a, i64 b, i64 c, i64 d, i64 e, i64 f) {
    i64 ret;
    register i64 r10 __asm__("r10") = d;
    register i64 r8 __asm__("r8") = e;
    register i64 r9 __asm__("r9") = f;
    __asm__ volatile("syscall"
            : "=a"(ret)
            : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8), "r"(r9)
            : "rcx", "r11", "memory");
    return ret;
}

static void write_all(const char *s, u64 n) {
    syscall6(1, 1, (i64) s, n, 0, 0, 0);
}

static void exit_with(i64 code) {
    syscall6(60, code, 0, 0, 0, 0, 0);
    for (;;) {}
}

static int fail;

static void check(const char *name, i64 ret) {
    if (ret < 0) {
        write_all(name, 4);
        write_all("\n", 1);
        fail = 1;
    }
}

struct sockaddr_in_ {
    uint16_t family;
    uint16_t port;
    uint32_t addr;
    uint8_t zero[8];
};

static uint16_t bswap16(uint16_t v) {
    return (uint16_t) ((v << 8) | (v >> 8));
}

void _start(void) {
    int srv = syscall6(41, 2, 1 | 0x800 | 0x80000, 0, 0, 0, 0);
    check("sock", srv);

    struct sockaddr_in_ sa = {
        .family = 2,
        .port = 0,
        .addr = 0x0100007f,
    };
    check("bind", syscall6(49, srv, (i64) &sa, sizeof(sa), 0, 0, 0));
    check("list", syscall6(50, srv, 4, 0, 0, 0, 0));
    u64 sa_len = sizeof(sa);
    check("name", syscall6(51, srv, (i64) &sa, (i64) &sa_len, 0, 0, 0));

    int cli = syscall6(41, 2, 1, 0, 0, 0, 0);
    check("soc2", cli);
    check("conn", syscall6(42, cli, (i64) &sa, sizeof(sa), 0, 0, 0));

    int acc = syscall6(43, srv, 0, 0, 0, 0, 0);
    check("acpt", acc);

    const char msg[] = "ping";
    char buf[8] = {};
    check("send", syscall6(44, cli, (i64) msg, 4, 0, 0, 0));
    check("recv", syscall6(45, acc, (i64) buf, sizeof(buf), 0, 0, 0));
    if (buf[0] != 'p' || buf[1] != 'i' || buf[2] != 'n' || buf[3] != 'g') {
        write_all("data\n", 5);
        fail = 1;
    }

    if (fail)
        exit_with(1);
    write_all("ok\n", 3);
    exit_with(0);
}
