#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE=alpine

if [[ $# -gt 0 ]]; then
    case "$1" in
        --alpine)
            MODE=alpine
            shift
            ;;
        --smoke)
            MODE=smoke
            shift
            ;;
    esac
fi

OUT="${2:-$REPO_ROOT/build/ish-x86_64-devroot.tar.gz}"

if [[ -z "${CLANG:-}" ]]; then
    if [[ -x /opt/homebrew/opt/llvm/bin/clang ]]; then
        CLANG=/opt/homebrew/opt/llvm/bin/clang
    else
        CLANG=clang
    fi
fi

compile_static_init() {
    local out="$1"
    local workdir
    workdir="$(mktemp -d "${TMPDIR:-/tmp}/ish-x64-init-src.XXXXXX")"

    cat > "$workdir/init.S" <<'ASM'
.global _start
.text
_start:
    mov $1, %rax
    mov $1, %rdi
    lea msg(%rip), %rsi
    mov $msg_len, %rdx
    syscall

sleep_forever:
    mov $35, %rax
    lea ts(%rip), %rdi
    xor %rsi, %rsi
    syscall
    jmp sleep_forever

.section .rodata
msg:
    .ascii "iSH x86_64 dev init alive\n"
.set msg_len, . - msg

.data
.align 8
ts:
    .quad 3600
    .quad 0
ASM

    "$CLANG" -target x86_64-linux-gnu -fuse-ld=lld -nostdlib -static \
        -Wl,-m,elf_x86_64 -o "$out" "$workdir/init.S"
    rm -rf "$workdir"
}

compile_static_shell() {
    local out="$1"
    local workdir
    workdir="$(mktemp -d "${TMPDIR:-/tmp}/ish-x64-shell-src.XXXXXX")"

    cat > "$workdir/ish-sh.S" <<'ASM'
.global _start
.text
_start:
    lea banner(%rip), %rsi
    mov $banner_len, %rdx
    call write_stdout

read_loop:
    lea prompt(%rip), %rsi
    mov $prompt_len, %rdx
    call write_stdout

    xor %rax, %rax
    xor %rdi, %rdi
    lea line(%rip), %rsi
    mov $1023, %rdx
    syscall
    test %rax, %rax
    jg got_line
    call sleep_short
    jmp read_loop

got_line:
    lea line(%rip), %r12
    lea (%r12,%rax), %r13
    movb $0, (%r13)

trim_tail:
    cmp %r12, %r13
    je skip_leading
    lea -1(%r13), %r14
    movb (%r14), %al
    cmp $10, %al
    je drop_tail
    cmp $13, %al
    je drop_tail
    jmp skip_leading

drop_tail:
    movb $0, (%r14)
    mov %r14, %r13
    jmp trim_tail

skip_leading:
    movb (%r12), %al
    cmp $32, %al
    je skip_one
    cmp $9, %al
    je skip_one
    cmp $0, %al
    je read_loop
    jmp check_exit

skip_one:
    inc %r12
    jmp skip_leading

check_exit:
    cmpb $101, 0(%r12)
    jne check_cd
    cmpb $120, 1(%r12)
    jne check_cd
    cmpb $105, 2(%r12)
    jne check_cd
    cmpb $116, 3(%r12)
    jne check_cd
    movb 4(%r12), %al
    cmp $0, %al
    je exit_ok
    cmp $32, %al
    je exit_ok
    cmp $9, %al
    jne check_cd

exit_ok:
    mov $60, %rax
    xor %rdi, %rdi
    syscall

check_cd:
    cmpb $99, 0(%r12)
    jne run_command
    cmpb $100, 1(%r12)
    jne run_command
    movb 2(%r12), %al
    cmp $0, %al
    je cd_home
    cmp $32, %al
    je cd_arg_start
    cmp $9, %al
    jne run_command

cd_arg_start:
    lea 2(%r12), %rdi
cd_skip:
    movb (%rdi), %al
    cmp $32, %al
    je cd_skip_one
    cmp $9, %al
    je cd_skip_one
    cmp $0, %al
    je cd_home
    jmp do_cd

cd_skip_one:
    inc %rdi
    jmp cd_skip

cd_home:
    lea home_path(%rip), %rdi

do_cd:
    mov $80, %rax
    syscall
    test %rax, %rax
    jns read_loop
    lea cd_error(%rip), %rsi
    mov $cd_error_len, %rdx
    call write_stderr
    jmp read_loop

run_command:
    mov $57, %rax
    syscall
    test %rax, %rax
    js fork_failed
    je child_exec

    mov %rax, %rbx
wait_child:
    mov $61, %rax
    mov %rbx, %rdi
    lea child_status(%rip), %rsi
    xor %rdx, %rdx
    xor %r10, %r10
    syscall
    jmp read_loop

child_exec:
    lea argv(%rip), %rsi
    mov %r12, 16(%rsi)
    lea shell_path(%rip), %rdi
    lea envp(%rip), %rdx
    mov $59, %rax
    syscall

    lea exec_error(%rip), %rsi
    mov $exec_error_len, %rdx
    call write_stderr
    mov $60, %rax
    mov $127, %rdi
    syscall

fork_failed:
    lea fork_error(%rip), %rsi
    mov $fork_error_len, %rdx
    call write_stderr
    jmp read_loop

write_stdout:
    mov $1, %rax
    mov $1, %rdi
    syscall
    ret

write_stderr:
    mov $1, %rax
    mov $2, %rdi
    syscall
    ret

sleep_short:
    mov $35, %rax
    lea sleep_ts(%rip), %rdi
    xor %rsi, %rsi
    syscall
    ret

.section .rodata
banner:
    .ascii "Welcome to Alpine x86_64 on iSH.\n"
    .ascii "This temporary launcher runs each line through /bin/sh -c.\n"
.set banner_len, . - banner

prompt:
    .ascii "iSH-x64:/# "
.set prompt_len, . - prompt

cd_error:
    .ascii "cd: could not change directory\n"
.set cd_error_len, . - cd_error

fork_error:
    .ascii "ish-sh: fork failed\n"
.set fork_error_len, . - fork_error

exec_error:
    .ascii "ish-sh: exec /bin/sh failed\n"
.set exec_error_len, . - exec_error

shell_path:
    .asciz "/bin/sh"
dash_c:
    .asciz "-c"
home_path:
    .asciz "/root"
env_term:
    .asciz "TERM=xterm-256color"
env_path:
    .asciz "PATH=/sbin:/usr/sbin:/bin:/usr/bin"
env_home:
    .asciz "HOME=/root"
env_user:
    .asciz "USER=root"
env_shell:
    .asciz "SHELL=/bin/sh"

.data
.align 8
argv:
    .quad shell_path
    .quad dash_c
    .quad 0
    .quad 0
envp:
    .quad env_term
    .quad env_path
    .quad env_home
    .quad env_user
    .quad env_shell
    .quad 0
sleep_ts:
    .quad 1
    .quad 0

.bss
.align 8
child_status:
    .quad 0
.align 16
line:
    .skip 1024
ASM

    "$CLANG" -target x86_64-linux-gnu -fuse-ld=lld -nostdlib -static \
        -Wl,-m,elf_x86_64 -o "$out" "$workdir/ish-sh.S"
    rm -rf "$workdir"
}

configure_alpine_repositories() {
    local root="$1"
    local release_file="$root/etc/alpine-release"
    if [[ ! -f "$release_file" ]]; then
        echo "Alpine rootfs is missing /etc/alpine-release" >&2
        exit 1
    fi

    local release
    local branch
    release="$(tr -d '[:space:]' < "$release_file")"
    branch="v${release%.*}"

    mkdir -p "$root/etc/apk"
    cat > "$root/etc/apk/repositories" <<EOF
https://dl-cdn.alpinelinux.org/alpine/$branch/main
https://dl-cdn.alpinelinux.org/alpine/$branch/community
EOF
}

configure_dns() {
    local root="$1"

    if [[ -e "$root/etc/resolv.conf" ]]; then
        return
    fi

    cat > "$root/etc/resolv.conf" <<'EOF'
nameserver 1.1.1.1
nameserver 8.8.8.8
EOF
}

prepare_apk_database() {
    local root="$1"
    local db="$root/lib/apk/db"

    mkdir -p "$db"

    # Alpine 3.23 ships apk-tools v3 while the minirootfs still carries a v2
    # text installed database. apk v3 refuses that database before `apk update`
    # can do anything, so keep the old metadata for later conversion work and
    # start this dev image with an empty v3 database.
    if [[ -f "$db/installed" && ! -f "$db/installed.v2" ]]; then
        mv "$db/installed" "$db/installed.v2"
    else
        rm -f "$db/installed"
    fi
    : > "$db/installed.adb"
}

package_alpine() {
    local src="${1:-$REPO_ROOT/build/alpine-x86_64-rootfs}"
    local fallback_tar="$REPO_ROOT/build/alpine-minirootfs-3.23.3-x86_64.tar.gz"
    local staging
    staging="$(mktemp -d "${TMPDIR:-/tmp}/ish-x64-alpine-root.XXXXXX")"

    if [[ -d "$src" ]]; then
        tar -C "$src" -cf - . | tar -C "$staging" -xf -
    elif [[ -f "$src" ]]; then
        tar -C "$staging" -xzf "$src"
    elif [[ "$src" == "$REPO_ROOT/build/alpine-x86_64-rootfs" && -f "$fallback_tar" ]]; then
        tar -C "$staging" -xzf "$fallback_tar"
    else
        cat >&2 <<EOF
Missing Alpine x86_64 rootfs source.
Expected a directory at:
  $src
or a tarball at:
  $fallback_tar
EOF
        exit 1
    fi

    for required in bin/sh bin/busybox lib/ld-musl-x86_64.so.1; do
        if [[ ! -e "$staging/$required" && ! -L "$staging/$required" ]]; then
            echo "Alpine rootfs is missing required path: /$required" >&2
            exit 1
        fi
    done

    mkdir -p "$staging/sbin" "$(dirname "$OUT")"
    rm -f "$staging/bin/ish-sh"
    configure_alpine_repositories "$staging"
    configure_dns "$staging"
    prepare_apk_database "$staging"
    # Keep app boot quiet and stable while the terminal launches the normal shell.
    compile_static_init "$staging/sbin/init"

    tar -czf "$OUT" -C "$staging" .
    rm -rf "$staging"
    echo "Wrote Alpine x86_64 dev rootfs with static init to $OUT"
}

package_smoke() {
    local ROOTFS="${1:-$REPO_ROOT/build/ish-x64-rootfs}"

    case "$ROOTFS" in
        /tmp/ish-x64-rootfs|/tmp/ish-x64-rootfs/*|*/build/ish-x64-rootfs|*/build/ish-x64-rootfs/*)
            rm -rf "$ROOTFS"
            ;;
        *)
            if [[ -e "$ROOTFS" ]]; then
                echo "Refusing to replace existing non-dev rootfs path: $ROOTFS" >&2
                exit 1
            fi
            ;;
    esac

    mkdir -p "$ROOTFS"/{bin,sbin,etc,dev,proc/ish,tmp,root} "$(dirname "$OUT")"
    chmod 1777 "$ROOTFS/tmp"

    cat > "$ROOTFS/etc/passwd" <<'PASSWD'
root:x:0:0:root:/root:/bin/sh
PASSWD

    cat > "$ROOTFS/etc/group" <<'GROUP'
root:x:0:
GROUP

    compile_static_init "$ROOTFS/sbin/init"

    local workdir
    workdir="$(mktemp -d "${TMPDIR:-/tmp}/ish-x64-devroot-src.XXXXXX")"

    cat > "$workdir/login.S" <<'ASM'
.global _start
.text
_start:
    mov $1, %rax
    mov $1, %rdi
    lea banner(%rip), %rsi
    mov $banner_len, %rdx
    syscall

read_loop:
    mov $1, %rax
    mov $1, %rdi
    lea prompt(%rip), %rsi
    mov $prompt_len, %rdx
    syscall

    mov $0, %rax
    mov $0, %rdi
    lea buf(%rip), %rsi
    mov $256, %rdx
    syscall

    cmp $0, %rax
    jg echo_input

    mov $35, %rax
    lea ts(%rip), %rdi
    xor %rsi, %rsi
    syscall
    jmp read_loop

echo_input:
    mov %rax, %rbx

    mov $1, %rax
    mov $1, %rdi
    lea echo(%rip), %rsi
    mov $echo_len, %rdx
    syscall

    mov $1, %rax
    mov $1, %rdi
    lea buf(%rip), %rsi
    mov %rbx, %rdx
    syscall

    jmp read_loop

.section .rodata
banner:
    .ascii "iSH x86_64 smoke shell ready\n"
    .ascii "Type anything and this static ELF64 guest will echo it.\n"
.set banner_len, . - banner

prompt:
    .ascii "x64> "
.set prompt_len, . - prompt

echo:
    .ascii "echo: "
.set echo_len, . - echo

.data
.align 8
ts:
    .quad 3600
    .quad 0

.bss
.align 16
buf:
    .skip 256
ASM

    "$CLANG" -target x86_64-linux-gnu -fuse-ld=lld -nostdlib -static \
        -Wl,-m,elf_x86_64 -o "$ROOTFS/bin/login" "$workdir/login.S"
    rm -rf "$workdir"
    cp "$ROOTFS/bin/login" "$ROOTFS/bin/sh"

    tar -czf "$OUT" -C "$ROOTFS" .
    echo "Wrote smoke x86_64 dev rootfs to $OUT"
}

case "$MODE" in
    alpine)
        package_alpine "${1:-}"
        ;;
    smoke)
        package_smoke "${1:-}"
        ;;
    *)
        echo "Unknown mode: $MODE" >&2
        exit 1
        ;;
esac
