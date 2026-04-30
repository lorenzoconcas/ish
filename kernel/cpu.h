#ifndef KERNEL_CPU_H
#define KERNEL_CPU_H

#include "kernel/task.h"

static inline const struct guest_abi *task_cpu_abi(const struct task *task) {
    return mm_guest_abi(task->mm);
}

// Until long-mode register state exists, both guest ABIs still route through
// the compat register file. Keeping that knowledge here shrinks the future
// x86_64 churn to one boundary.
static inline dword_t task_cpu_compat_reg(const struct task *task, enum reg32 reg) {
    switch (task_cpu_abi(task)->arch) {
        case GUEST_ARCH_X86_32:
        case GUEST_ARCH_X86_64:
            return cpu_compat_gpr(&task->cpu, reg);
    }
    return cpu_compat_gpr(&task->cpu, reg);
}

static inline void task_cpu_set_compat_reg(struct task *task, enum reg32 reg, addr_t value) {
    switch (task_cpu_abi(task)->arch) {
        case GUEST_ARCH_X86_32:
        case GUEST_ARCH_X86_64:
            cpu_set_compat_gpr(&task->cpu, reg, value);
            return;
    }
}

static inline addr_t task_cpu_instruction_pointer(const struct task *task) {
    switch (task_cpu_abi(task)->arch) {
        case GUEST_ARCH_X86_32:
            return cpu_compat_ip(&task->cpu);
        case GUEST_ARCH_X86_64:
            return cpu_ip64(&task->cpu);
    }
    return cpu_compat_ip(&task->cpu);
}

static inline void task_cpu_set_instruction_pointer(struct task *task, addr_t ip) {
    switch (task_cpu_abi(task)->arch) {
        case GUEST_ARCH_X86_32:
            cpu_set_compat_ip(&task->cpu, ip);
            return;
        case GUEST_ARCH_X86_64:
            cpu_set_ip64(&task->cpu, ip);
            return;
    }
}

static inline addr_t task_cpu_stack_pointer(const struct task *task) {
    switch (task_cpu_abi(task)->arch) {
        case GUEST_ARCH_X86_32:
            return cpu_compat_sp(&task->cpu);
        case GUEST_ARCH_X86_64:
            return cpu_gpr64(&task->cpu, reg_rsp);
    }
    return cpu_compat_sp(&task->cpu);
}

static inline void task_cpu_set_stack_pointer(struct task *task, addr_t sp) {
    switch (task_cpu_abi(task)->arch) {
        case GUEST_ARCH_X86_32:
            cpu_set_compat_sp(&task->cpu, sp);
            return;
        case GUEST_ARCH_X86_64:
            cpu_set_gpr64(&task->cpu, reg_rsp, sp);
            return;
    }
}

static inline void task_cpu_reset_exec_state(struct task *task, addr_t sp, addr_t ip) {
    switch (task_cpu_abi(task)->arch) {
        case GUEST_ARCH_X86_32:
            cpu_zero_compat_gprs(&task->cpu);
            cpu_set_compat_sp(&task->cpu, sp);
            cpu_set_compat_ip(&task->cpu, ip);
            return;
        case GUEST_ARCH_X86_64:
            cpu_zero_gprs64(&task->cpu);
            cpu_set_gpr64(&task->cpu, reg_rsp, sp);
            cpu_set_ip64(&task->cpu, ip);
            return;
    }
}

static inline addr_t task_cpu_syscall_number(const struct task *task) {
    switch (task_cpu_abi(task)->arch) {
        case GUEST_ARCH_X86_32:
            return task_cpu_compat_reg(task, reg_eax);
        case GUEST_ARCH_X86_64:
            return cpu_gpr64(&task->cpu, reg_rax);
    }
    return task_cpu_compat_reg(task, reg_eax);
}

static inline addr_t task_cpu_syscall_arg(const struct task *task, unsigned index) {
    static const enum reg32 compat_syscall_regs[] = {
        reg_ebx,
        reg_ecx,
        reg_edx,
        reg_esi,
        reg_edi,
        reg_ebp,
    };
    static const enum reg64 long_mode_syscall_regs[] = {
        reg_rdi,
        reg_rsi,
        reg_rdx,
        reg_r10,
        reg_r8,
        reg_r9,
    };
    assert(index < sizeof(compat_syscall_regs) / sizeof(compat_syscall_regs[0]));
    switch (task_cpu_abi(task)->arch) {
        case GUEST_ARCH_X86_32:
            return task_cpu_compat_reg(task, compat_syscall_regs[index]);
        case GUEST_ARCH_X86_64:
            return cpu_gpr64(&task->cpu, long_mode_syscall_regs[index]);
    }
    return task_cpu_compat_reg(task, compat_syscall_regs[index]);
}

static inline addr_t task_cpu_normalize_syscall_result(addr_t result) {
    if (result <= UINT32_MAX && (dword_t) result >= (dword_t) -4095)
        return (addr_t) (sqword_t) (sdword_t) (dword_t) result;
    return result;
}

static inline void task_cpu_set_syscall_result(struct task *task, addr_t result) {
    result = task_cpu_normalize_syscall_result(result);
    switch (task_cpu_abi(task)->arch) {
        case GUEST_ARCH_X86_32:
            cpu_set_compat_gpr(&task->cpu, reg_eax, (dword_t) result);
            return;
        case GUEST_ARCH_X86_64:
            cpu_set_gpr64(&task->cpu, reg_rax, result);
            return;
    }
}

static inline qword_t task_cpu_gpr64(const struct task *task, enum reg64 reg) {
    return cpu_gpr64(&task->cpu, reg);
}

static inline void task_cpu_set_gpr64(struct task *task, enum reg64 reg, qword_t value) {
    cpu_set_gpr64(&task->cpu, reg, value);
}

static inline addr_t task_cpu_instruction_pointer64(const struct task *task) {
    return cpu_ip64(&task->cpu);
}

static inline void task_cpu_set_instruction_pointer64(struct task *task, addr_t ip) {
    cpu_set_ip64(&task->cpu, ip);
}

static inline addr_t task_cpu_compat_tls_base(const struct task *task) {
    return task->cpu.tls_ptr;
}

static inline void task_cpu_set_compat_tls_base(struct task *task, addr_t base) {
    task->cpu.tls_ptr = base;
    task->cpu.gsbase = base;
}

static inline addr_t task_cpu_fs_base(const struct task *task) {
    return task->cpu.fsbase;
}

static inline void task_cpu_set_fs_base(struct task *task, addr_t base) {
    task->cpu.fsbase = base;
}

static inline addr_t task_cpu_gs_base(const struct task *task) {
    return task->cpu.gsbase;
}

static inline void task_cpu_set_gs_base(struct task *task, addr_t base) {
    task->cpu.gsbase = base;
    task->cpu.tls_ptr = base;
}

#endif
