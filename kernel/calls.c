#include <string.h>
#include "debug.h"
#include "kernel/calls.h"
#include "kernel/cpu.h"
#include "emu/interrupt.h"
#include "kernel/memory.h"
#include "kernel/signal.h"
#include "kernel/task.h"

dword_t syscall_stub(void) {
    return _ENOSYS;
}
// While identical, this version of the stub doesn't log below. Use this for
// syscalls that are optional (i.e. fallback on something else) but called
// frequently.
dword_t syscall_silent_stub(void) {
    return _ENOSYS;
}
dword_t syscall_success_stub(void) {
    return 0;
}

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-function-type-mismatch"
#elif is_gcc(8)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
static syscall_t syscall_table_x86_32[] = {
    [1]   = (syscall_t) sys_exit,
    [2]   = (syscall_t) sys_fork,
    [3]   = (syscall_t) sys_read,
    [4]   = (syscall_t) sys_write,
    [5]   = (syscall_t) sys_open,
    [6]   = (syscall_t) sys_close,
    [7]   = (syscall_t) sys_waitpid,
    [9]   = (syscall_t) sys_link,
    [10]  = (syscall_t) sys_unlink,
    [11]  = (syscall_t) sys_execve,
    [12]  = (syscall_t) sys_chdir,
    [13]  = (syscall_t) sys_time,
    [14]  = (syscall_t) sys_mknod,
    [15]  = (syscall_t) sys_chmod,
    [19]  = (syscall_t) sys_lseek,
    [20]  = (syscall_t) sys_getpid,
    [21]  = (syscall_t) sys_mount,
    [23]  = (syscall_t) sys_setuid,
    [24]  = (syscall_t) sys_getuid,
    [25]  = (syscall_t) sys_stime,
    [26]  = (syscall_t) sys_ptrace,
    [27]  = (syscall_t) sys_alarm,
    [29]  = (syscall_t) sys_pause,
    [30]  = (syscall_t) sys_utime,
    [33]  = (syscall_t) sys_access,
    [36]  = (syscall_t) syscall_success_stub, // sync
    [37]  = (syscall_t) sys_kill,
    [38]  = (syscall_t) sys_rename,
    [39]  = (syscall_t) sys_mkdir,
    [40]  = (syscall_t) sys_rmdir,
    [41]  = (syscall_t) sys_dup,
    [42]  = (syscall_t) sys_pipe,
    [43]  = (syscall_t) sys_times,
    [45]  = (syscall_t) sys_brk,
    [46]  = (syscall_t) sys_setgid,
    [47]  = (syscall_t) sys_getgid,
    [49]  = (syscall_t) sys_geteuid,
    [50]  = (syscall_t) sys_getegid,
    [52]  = (syscall_t) sys_umount2,
    [54]  = (syscall_t) sys_ioctl,
    [55]  = (syscall_t) sys_fcntl32,
    [57]  = (syscall_t) sys_setpgid,
    [60]  = (syscall_t) sys_umask,
    [61]  = (syscall_t) sys_chroot,
    [63]  = (syscall_t) sys_dup2,
    [64]  = (syscall_t) sys_getppid,
    [65]  = (syscall_t) sys_getpgrp,
    [66]  = (syscall_t) sys_setsid,
    [74]  = (syscall_t) sys_sethostname,
    [75]  = (syscall_t) sys_setrlimit32,
    [76]  = (syscall_t) sys_old_getrlimit32,
    [77]  = (syscall_t) sys_getrusage,
    [78]  = (syscall_t) sys_gettimeofday,
    [79]  = (syscall_t) sys_settimeofday,
    [80]  = (syscall_t) sys_getgroups,
    [81]  = (syscall_t) sys_setgroups,
    [83]  = (syscall_t) sys_symlink,
    [85]  = (syscall_t) sys_readlink,
    [88]  = (syscall_t) sys_reboot,
    [90]  = (syscall_t) sys_mmap,
    [91]  = (syscall_t) sys_munmap,
    [94]  = (syscall_t) sys_fchmod,
    [96]  = (syscall_t) sys_getpriority,
    [97]  = (syscall_t) sys_setpriority,
    [99]  = (syscall_t) sys_statfs,
    [100] = (syscall_t) sys_fstatfs,
    [102] = (syscall_t) sys_socketcall,
    [103] = (syscall_t) sys_syslog,
    [104] = (syscall_t) sys_setitimer,
    [114] = (syscall_t) sys_wait4,
    [116] = (syscall_t) sys_sysinfo,
    [117] = (syscall_t) sys_ipc,
    [118] = (syscall_t) sys_fsync,
    [119] = (syscall_t) sys_sigreturn,
    [120] = (syscall_t) sys_clone,
    [122] = (syscall_t) sys_uname,
    [125] = (syscall_t) sys_mprotect,
    [132] = (syscall_t) sys_getpgid,
    [133] = (syscall_t) sys_fchdir,
    [136] = (syscall_t) sys_personality,
    [140] = (syscall_t) sys__llseek,
    [141] = (syscall_t) sys_getdents,
    [142] = (syscall_t) sys_select,
    [143] = (syscall_t) sys_flock,
    [144] = (syscall_t) sys_msync,
    [145] = (syscall_t) sys_readv,
    [146] = (syscall_t) sys_writev,
    [147] = (syscall_t) sys_getsid,
    [148] = (syscall_t) sys_fsync, // fdatasync
    [150] = (syscall_t) sys_mlock,
    [155] = (syscall_t) sys_sched_getparam,
    [156] = (syscall_t) sys_sched_setscheduler,
    [157] = (syscall_t) sys_sched_getscheduler,
    [158] = (syscall_t) sys_sched_yield,
    [159] = (syscall_t) sys_sched_get_priority_max,
    [162] = (syscall_t) sys_nanosleep,
    [163] = (syscall_t) sys_mremap,
    [168] = (syscall_t) sys_poll,
    [172] = (syscall_t) sys_prctl,
    [173] = (syscall_t) sys_rt_sigreturn,
    [174] = (syscall_t) sys_rt_sigaction,
    [175] = (syscall_t) sys_rt_sigprocmask,
    [176] = (syscall_t) sys_rt_sigpending,
    [177] = (syscall_t) sys_rt_sigtimedwait,
    [179] = (syscall_t) sys_rt_sigsuspend,
    [180] = (syscall_t) sys_pread,
    [181] = (syscall_t) sys_pwrite,
    [183] = (syscall_t) sys_getcwd,
    [184] = (syscall_t) sys_capget,
    [185] = (syscall_t) sys_capset,
    [186] = (syscall_t) sys_sigaltstack,
    [187] = (syscall_t) sys_sendfile,
    [190] = (syscall_t) sys_vfork,
    [191] = (syscall_t) sys_getrlimit32,
    [192] = (syscall_t) sys_mmap2,
    [193] = (syscall_t) sys_truncate64,
    [194] = (syscall_t) sys_ftruncate64,
    [195] = (syscall_t) sys_stat64,
    [196] = (syscall_t) sys_lstat64,
    [197] = (syscall_t) sys_fstat64,
    [198] = (syscall_t) sys_lchown,
    [199] = (syscall_t) sys_getuid32,
    [200] = (syscall_t) sys_getgid32,
    [201] = (syscall_t) sys_geteuid32,
    [202] = (syscall_t) sys_getegid32,
    [203] = (syscall_t) sys_setreuid,
    [204] = (syscall_t) sys_setregid,
    [205] = (syscall_t) sys_getgroups,
    [206] = (syscall_t) sys_setgroups,
    [207] = (syscall_t) sys_fchown32,
    [208] = (syscall_t) sys_setresuid,
    [209] = (syscall_t) sys_getresuid,
    [210] = (syscall_t) sys_setresgid,
    [211] = (syscall_t) sys_getresgid,
    [212] = (syscall_t) sys_chown32,
    [213] = (syscall_t) sys_setuid,
    [214] = (syscall_t) sys_setgid,
    [215] = (syscall_t) syscall_stub, // setfsuid
    [216] = (syscall_t) syscall_stub, // setfsgid
    [219] = (syscall_t) sys_madvise,
    [220] = (syscall_t) sys_getdents64,
    [221] = (syscall_t) sys_fcntl,
    [224] = (syscall_t) sys_gettid,
    [225] = (syscall_t) syscall_success_stub, // readahead
    [226 ... 237] = (syscall_t) sys_xattr_stub,
    [238] = (syscall_t) sys_tkill,
    [239] = (syscall_t) sys_sendfile64,
    [240] = (syscall_t) sys_futex,
    [241] = (syscall_t) sys_sched_setaffinity,
    [242] = (syscall_t) sys_sched_getaffinity,
    [243] = (syscall_t) sys_set_thread_area,
    [245] = (syscall_t) syscall_stub, // io_setup
    [252] = (syscall_t) sys_exit_group,
    [254] = (syscall_t) sys_epoll_create0,
    [255] = (syscall_t) sys_epoll_ctl,
    [256] = (syscall_t) sys_epoll_wait,
    [258] = (syscall_t) sys_set_tid_address,
    [259] = (syscall_t) sys_timer_create,
    [260] = (syscall_t) sys_timer_settime,
    [263] = (syscall_t) sys_timer_delete,
    [264] = (syscall_t) sys_clock_settime,
    [265] = (syscall_t) sys_clock_gettime,
    [266] = (syscall_t) sys_clock_getres,
    [268] = (syscall_t) sys_statfs64,
    [269] = (syscall_t) sys_fstatfs64,
    [270] = (syscall_t) sys_tgkill,
    [271] = (syscall_t) sys_utimes,
    [272] = (syscall_t) syscall_success_stub,
    [274] = (syscall_t) sys_mbind,
    [284] = (syscall_t) sys_waitid,
    [289] = (syscall_t) sys_ioprio_set,
    [290] = (syscall_t) sys_ioprio_get,
    [291] = (syscall_t) syscall_stub, // inotify_init
    [295] = (syscall_t) sys_openat,
    [296] = (syscall_t) sys_mkdirat,
    [297] = (syscall_t) sys_mknodat,
    [298] = (syscall_t) sys_fchownat,
    [300] = (syscall_t) sys_fstatat64,
    [301] = (syscall_t) sys_unlinkat,
    [302] = (syscall_t) sys_renameat,
    [303] = (syscall_t) sys_linkat,
    [304] = (syscall_t) sys_symlinkat,
    [305] = (syscall_t) sys_readlinkat,
    [306] = (syscall_t) sys_fchmodat,
    [307] = (syscall_t) sys_faccessat,
    [308] = (syscall_t) sys_pselect,
    [309] = (syscall_t) sys_ppoll,
    [311] = (syscall_t) sys_set_robust_list,
    [312] = (syscall_t) sys_get_robust_list,
    [313] = (syscall_t) sys_splice,
    [319] = (syscall_t) sys_epoll_pwait,
    [320] = (syscall_t) sys_utimensat,
    [322] = (syscall_t) sys_timerfd_create,
    [323] = (syscall_t) sys_eventfd,
    [324] = (syscall_t) sys_fallocate,
    [325] = (syscall_t) sys_timerfd_settime,
    [328] = (syscall_t) sys_eventfd2,
    [329] = (syscall_t) sys_epoll_create,
    [330] = (syscall_t) sys_dup3,
    [331] = (syscall_t) sys_pipe2,
    [332] = (syscall_t) syscall_stub, // inotify_init1
    [340] = (syscall_t) sys_prlimit64,
    [345] = (syscall_t) sys_sendmmsg,
    [352] = (syscall_t) syscall_stub, // sched_getattr
    [353] = (syscall_t) sys_renameat2,
    [355] = (syscall_t) sys_getrandom,
    [359] = (syscall_t) sys_socket,
    [360] = (syscall_t) sys_socketpair,
    [361] = (syscall_t) sys_bind,
    [362] = (syscall_t) sys_connect,
    [363] = (syscall_t) sys_listen,
    [364] = (syscall_t) syscall_stub, // accept4
    [365] = (syscall_t) sys_getsockopt,
    [366] = (syscall_t) sys_setsockopt,
    [367] = (syscall_t) sys_getsockname,
    [368] = (syscall_t) sys_getpeername,
    [369] = (syscall_t) sys_sendto,
    [370] = (syscall_t) sys_sendmsg,
    [371] = (syscall_t) sys_recvfrom,
    [372] = (syscall_t) sys_recvmsg,
    [373] = (syscall_t) sys_shutdown,
    [375] = (syscall_t) syscall_silent_stub, // membarrier
    [377] = (syscall_t) sys_copy_file_range,
    [383] = (syscall_t) sys_statx,
    [384] = (syscall_t) sys_arch_prctl,
    [422] = (syscall_t) syscall_silent_stub, // futex_time64
    [439] = (syscall_t) syscall_silent_stub, // faccessat2
};

static syscall_t lookup_syscall_x86_32(addr_t syscall_num) {
    size_t num_syscalls = sizeof(syscall_table_x86_32) / sizeof(syscall_table_x86_32[0]);
    if (syscall_num >= num_syscalls)
        return NULL;
    return syscall_table_x86_32[syscall_num];
}

static syscall_t lookup_syscall_x86_64(addr_t syscall_num) {
    // Numbering follows Linux's official x86_64 table in
    // arch/x86/entry/syscalls/syscall_64.tbl.
    // Only syscalls whose argument order and userspace ABI already match the
    // current runtime are wired here. mmap/clone/signal-entry work still need
    // dedicated x86_64 handling before they can be enabled safely.
#define X64_SYSCALL(num, fn) case num: return (syscall_t) fn
    switch (syscall_num) {
        X64_SYSCALL(0, sys_read);
        X64_SYSCALL(1, sys_write);
        X64_SYSCALL(2, sys_open);
        X64_SYSCALL(3, sys_close);
        X64_SYSCALL(4, sys_stat_x86_64);
        X64_SYSCALL(5, sys_fstat_x86_64);
        X64_SYSCALL(6, sys_lstat_x86_64);
        X64_SYSCALL(7, sys_poll);
        X64_SYSCALL(8, sys_lseek_x86_64);
        X64_SYSCALL(9, sys_mmap64);
        X64_SYSCALL(10, sys_mprotect);
        X64_SYSCALL(11, sys_munmap);
        X64_SYSCALL(12, sys_brk);
        X64_SYSCALL(13, sys_rt_sigaction_x86_64);
        X64_SYSCALL(14, sys_rt_sigprocmask);
        X64_SYSCALL(16, sys_ioctl);
        X64_SYSCALL(19, sys_readv);
        X64_SYSCALL(20, sys_writev);
        X64_SYSCALL(23, sys_select_x86_64);
        X64_SYSCALL(21, sys_access);
        X64_SYSCALL(22, sys_pipe);
        X64_SYSCALL(24, sys_sched_yield);
        X64_SYSCALL(25, sys_mremap);
        X64_SYSCALL(32, sys_dup);
        X64_SYSCALL(33, sys_dup2);
        X64_SYSCALL(35, sys_nanosleep_x86_64);
        X64_SYSCALL(39, sys_getpid);
        X64_SYSCALL(40, sys_sendfile64);
        X64_SYSCALL(41, sys_socket);
        X64_SYSCALL(42, sys_connect);
        X64_SYSCALL(44, sys_sendto);
        X64_SYSCALL(45, sys_recvfrom);
        X64_SYSCALL(46, sys_sendmsg);
        X64_SYSCALL(47, sys_recvmsg);
        X64_SYSCALL(48, sys_shutdown);
        X64_SYSCALL(49, sys_bind);
        X64_SYSCALL(50, sys_listen);
        X64_SYSCALL(51, sys_getsockname);
        X64_SYSCALL(52, sys_getpeername);
        X64_SYSCALL(53, sys_socketpair);
        X64_SYSCALL(54, sys_setsockopt);
        X64_SYSCALL(55, sys_getsockopt);
        X64_SYSCALL(56, sys_clone_x86_64);
        X64_SYSCALL(57, sys_fork);
        X64_SYSCALL(58, sys_vfork);
        X64_SYSCALL(59, sys_execve);
        X64_SYSCALL(60, sys_exit);
        X64_SYSCALL(61, sys_wait4);
        X64_SYSCALL(62, sys_kill);
        X64_SYSCALL(63, sys_uname);
        X64_SYSCALL(72, sys_fcntl);
        X64_SYSCALL(73, sys_flock);
        X64_SYSCALL(74, sys_fsync);
        X64_SYSCALL(75, sys_fsync);
        X64_SYSCALL(79, sys_getcwd);
        X64_SYSCALL(80, sys_chdir);
        X64_SYSCALL(81, sys_fchdir);
        X64_SYSCALL(82, sys_rename);
        X64_SYSCALL(83, sys_mkdir);
        X64_SYSCALL(84, sys_rmdir);
        X64_SYSCALL(86, sys_link);
        X64_SYSCALL(87, sys_unlink);
        X64_SYSCALL(88, sys_symlink);
        X64_SYSCALL(89, sys_readlink);
        X64_SYSCALL(90, sys_chmod);
        X64_SYSCALL(91, sys_fchmod);
        X64_SYSCALL(92, sys_chown32);
        X64_SYSCALL(93, sys_fchown32);
        X64_SYSCALL(94, sys_lchown);
        X64_SYSCALL(95, sys_umask);
        X64_SYSCALL(96, sys_gettimeofday_x86_64);
        X64_SYSCALL(98, sys_getrusage);
        X64_SYSCALL(99, sys_sysinfo);
        X64_SYSCALL(100, sys_times);
        X64_SYSCALL(102, sys_getuid);
        X64_SYSCALL(104, sys_getgid);
        X64_SYSCALL(105, sys_setuid);
        X64_SYSCALL(106, sys_setgid);
        X64_SYSCALL(107, sys_geteuid);
        X64_SYSCALL(108, sys_getegid);
        X64_SYSCALL(109, sys_setpgid);
        X64_SYSCALL(110, sys_getppid);
        X64_SYSCALL(111, sys_getpgrp);
        X64_SYSCALL(112, sys_setsid);
        X64_SYSCALL(113, sys_setreuid);
        X64_SYSCALL(114, sys_setregid);
        X64_SYSCALL(115, sys_getgroups);
        X64_SYSCALL(116, sys_setgroups);
        X64_SYSCALL(117, sys_setresuid);
        X64_SYSCALL(118, sys_getresuid);
        X64_SYSCALL(119, sys_setresgid);
        X64_SYSCALL(120, sys_getresgid);
        X64_SYSCALL(121, sys_getpgid);
        X64_SYSCALL(124, sys_getsid);
        X64_SYSCALL(125, sys_capget);
        X64_SYSCALL(126, sys_capset);
        X64_SYSCALL(135, sys_personality);
        X64_SYSCALL(137, sys_statfs_x86_64);
        X64_SYSCALL(138, sys_fstatfs_x86_64);
        X64_SYSCALL(140, sys_getpriority);
        X64_SYSCALL(141, sys_setpriority);
        X64_SYSCALL(149, sys_mlock);
        X64_SYSCALL(157, sys_prctl);
        X64_SYSCALL(158, sys_arch_prctl);
        X64_SYSCALL(161, sys_chroot);
        X64_SYSCALL(169, sys_reboot);
        X64_SYSCALL(170, sys_sethostname);
        X64_SYSCALL(186, sys_gettid);
        X64_SYSCALL(200, sys_tkill);
        X64_SYSCALL(201, sys_time);
        X64_SYSCALL(202, sys_futex);
        X64_SYSCALL(203, sys_sched_setaffinity);
        X64_SYSCALL(204, sys_sched_getaffinity);
        X64_SYSCALL(217, sys_getdents64);
        X64_SYSCALL(218, sys_set_tid_address);
        X64_SYSCALL(222, sys_timer_create);
        X64_SYSCALL(223, sys_timer_settime);
        X64_SYSCALL(226, sys_timer_delete);
        X64_SYSCALL(227, sys_clock_settime);
        X64_SYSCALL(228, sys_clock_gettime_x86_64);
        X64_SYSCALL(229, sys_clock_getres_x86_64);
        X64_SYSCALL(231, sys_exit_group);
        X64_SYSCALL(232, sys_epoll_wait);
        X64_SYSCALL(233, sys_epoll_ctl);
        X64_SYSCALL(234, sys_tgkill);
        X64_SYSCALL(235, sys_utimes);
        X64_SYSCALL(237, sys_mbind);
        X64_SYSCALL(247, sys_waitid);
        X64_SYSCALL(251, sys_ioprio_set);
        X64_SYSCALL(252, sys_ioprio_get);
        X64_SYSCALL(257, sys_openat);
        X64_SYSCALL(258, sys_mkdirat);
        X64_SYSCALL(259, sys_mknodat);
        X64_SYSCALL(260, sys_fchownat);
        X64_SYSCALL(262, sys_newfstatat_x86_64);
        X64_SYSCALL(263, sys_unlinkat);
        X64_SYSCALL(264, sys_renameat);
        X64_SYSCALL(265, sys_linkat);
        X64_SYSCALL(266, sys_symlinkat);
        X64_SYSCALL(267, sys_readlinkat);
        X64_SYSCALL(268, sys_fchmodat);
        X64_SYSCALL(269, sys_faccessat);
        X64_SYSCALL(270, sys_pselect_x86_64);
        X64_SYSCALL(271, sys_ppoll_x86_64);
        X64_SYSCALL(273, sys_set_robust_list);
        X64_SYSCALL(274, sys_get_robust_list);
        X64_SYSCALL(280, sys_utimensat);
        X64_SYSCALL(281, sys_epoll_pwait);
        X64_SYSCALL(283, sys_timerfd_create);
        X64_SYSCALL(290, sys_eventfd2);
        X64_SYSCALL(291, sys_epoll_create);
        X64_SYSCALL(292, sys_dup3);
        X64_SYSCALL(293, sys_pipe2);
        X64_SYSCALL(302, sys_prlimit64);
        X64_SYSCALL(307, sys_sendmmsg);
        X64_SYSCALL(316, sys_renameat2);
        X64_SYSCALL(318, sys_getrandom);
        X64_SYSCALL(319, syscall_silent_stub);
        X64_SYSCALL(332, sys_statx);
        X64_SYSCALL(377, sys_copy_file_range);
        X64_SYSCALL(439, syscall_silent_stub);
        default:
            return NULL;
    }
#undef X64_SYSCALL
}

static syscall_t lookup_syscall(enum guest_arch arch, addr_t syscall_num) {
    switch (arch) {
        case GUEST_ARCH_X86_32:
            return lookup_syscall_x86_32(syscall_num);
        case GUEST_ARCH_X86_64:
            return lookup_syscall_x86_64(syscall_num);
    }
    return NULL;
}

void dump_stack(int lines);

void handle_interrupt(int interrupt) {
    struct cpu_state *cpu = &current->cpu;
    if (interrupt == INT_SYSCALL) {
        addr_t syscall_num = task_cpu_syscall_number(current);
        syscall_t syscall = lookup_syscall(task_cpu_abi(current)->arch, syscall_num);
        if (syscall == NULL) {
            printk("%d(%s) missing syscall %#llx\n", current->pid, current->comm,
                    (unsigned long long) syscall_num);
            task_cpu_set_syscall_result(current, _ENOSYS);
        } else {
            if (syscall == (syscall_t) syscall_stub) {
                printk("%d(%s) stub syscall %#llx\n", current->pid, current->comm,
                        (unsigned long long) syscall_num);
            }
            STRACE("%d call %-3llu ", current->pid, (unsigned long long) syscall_num);
            addr_t result = syscall(
                    task_cpu_syscall_arg(current, 0),
                    task_cpu_syscall_arg(current, 1),
                    task_cpu_syscall_arg(current, 2),
                    task_cpu_syscall_arg(current, 3),
                    task_cpu_syscall_arg(current, 4),
                    task_cpu_syscall_arg(current, 5));
            STRACE(" = 0x%llx\n", (unsigned long long) result);
            task_cpu_set_syscall_result(current, result);
        }
    } else if (interrupt == INT_GPF) {
        // some page faults, such as stack growing or CoW clones, are handled by mem_ptr
        read_wrlock(&current->mem->lock);
        void *ptr = mem_ptr(current->mem, cpu->segfault_addr, cpu->segfault_was_write ? MEM_WRITE : MEM_READ);
        read_wrunlock(&current->mem->lock);
        if (ptr == NULL) {
            addr_t ip = task_cpu_instruction_pointer(current);
            printk("%d page fault on %#llx at %#llx\n", current->pid,
                    (unsigned long long) cpu->segfault_addr,
                    (unsigned long long) ip);
            struct siginfo_ info = {
                .code = mem_segv_reason(current->mem, cpu->segfault_addr),
                .fault.addr = cpu->segfault_addr,
            };
            dump_stack(8);
            deliver_signal(current, SIGSEGV_, info);
        }
    } else if (interrupt == INT_UNDEFINED) {
        addr_t ip = task_cpu_instruction_pointer(current);
        printk("%d illegal instruction at %#llx: ", current->pid, (unsigned long long) ip);
        for (int i = 0; i < 8; i++) {
            uint8_t b;
            if (user_get(ip + i, b))
                break;
            printk("%02x ", b);
        }
        printk("\n");
        dump_stack(8);
        struct siginfo_ info = {
            .code = SI_KERNEL_,
            .fault.addr = ip,
        };
        deliver_signal(current, SIGILL_, info);
    } else if (interrupt == INT_BREAKPOINT) {
        lock(&pids_lock);
        send_signal(current, SIGTRAP_, (struct siginfo_) {
            .sig = SIGTRAP_,
            .code = SI_KERNEL_,
        });
        unlock(&pids_lock);
    } else if (interrupt == INT_DEBUG) {
        lock(&pids_lock);
        send_signal(current, SIGTRAP_, (struct siginfo_) {
            .sig = SIGTRAP_,
            .code = TRAP_TRACE_,
        });
        unlock(&pids_lock);
    } else if (interrupt != INT_TIMER) {
        printk("%d unhandled interrupt %d\n", current->pid, interrupt);
        sys_exit(interrupt);
    }

    receive_signals();
    struct tgroup *group = current->group;
    lock(&group->lock);
    while (group->stopped)
        wait_for_ignore_signals(&group->stopped_cond, &group->lock, NULL);
    unlock(&group->lock);
}

#if defined(__clang__)
#pragma clang diagnostic pop
#elif is_gcc(8)
#pragma GCC diagnostic pop
#endif

void dump_maps(void) {
    extern void proc_maps_dump(struct task *task, struct proc_data *buf);
    struct proc_data buf = {};
    proc_maps_dump(current, &buf);
    // go a line at a time because it can be fucking enormous
    char *orig_data = buf.data;
    while (buf.size > 0) {
        size_t chunk_size = buf.size;
        if (chunk_size > 1024)
            chunk_size = 1024;
        printk("%.*s", chunk_size, buf.data);
        buf.data += chunk_size;
        buf.size -= chunk_size;
    }
    free(orig_data);
}

void dump_mem(addr_t start, uint_t len) {
    const int width = 8;
    for (addr_t addr = start; addr < start + len; addr += sizeof(dword_t)) {
        unsigned from_left = (addr - start) / sizeof(dword_t) % width;
        if (from_left == 0)
            printk("%08x: ", addr);
        dword_t word;
        if (user_get(addr, word))
            break;
        printk("%08x ", word);
        if (from_left == width - 1)
            printk("\n");
    }
}

void dump_stack(int lines) {
    const struct guest_abi *abi = task_cpu_abi(current);
    if (abi->arch == GUEST_ARCH_X86_64) {
        struct cpu_state *cpu = &current->cpu;
        printk("stack at %llx, base at %llx, ip at %llx\n",
                (unsigned long long) cpu->rsp,
                (unsigned long long) cpu->rbp,
                (unsigned long long) cpu->rip);
        printk("rax=%016llx rbx=%016llx rcx=%016llx rdx=%016llx\n",
                (unsigned long long) cpu->rax,
                (unsigned long long) cpu->rbx,
                (unsigned long long) cpu->rcx,
                (unsigned long long) cpu->rdx);
        printk("rsi=%016llx rdi=%016llx rbp=%016llx rsp=%016llx\n",
                (unsigned long long) cpu->rsi,
                (unsigned long long) cpu->rdi,
                (unsigned long long) cpu->rbp,
                (unsigned long long) cpu->rsp);
        printk("r8 =%016llx r9 =%016llx r10=%016llx r11=%016llx\n",
                (unsigned long long) cpu->r8,
                (unsigned long long) cpu->r9,
                (unsigned long long) cpu->r10,
                (unsigned long long) cpu->r11);
        printk("r12=%016llx r13=%016llx r14=%016llx r15=%016llx\n",
                (unsigned long long) cpu->r12,
                (unsigned long long) cpu->r13,
                (unsigned long long) cpu->r14,
                (unsigned long long) cpu->r15);
        printk("eflags=%08x flags_res=%02x res=%016llx cf=%u of=%u zf=%u sf=%u pf=%u\n",
                cpu->eflags,
                cpu->flags_res,
                (unsigned long long) cpu->res,
                cpu->cf,
                cpu->of,
                ZF ? 1 : 0,
                SF ? 1 : 0,
                PF ? 1 : 0);
        dump_mem((addr_t) cpu->rsp, lines * sizeof(dword_t) * 8);
        return;
    }
    printk("stack at %x, base at %x, ip at %x\n", current->cpu.esp, current->cpu.ebp, current->cpu.eip);
    dump_mem(current->cpu.esp, lines * sizeof(dword_t) * 8);
}

// TODO find a home for this
#ifdef LOG_OVERRIDE
int log_override = 0;
#endif
