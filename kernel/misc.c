#include <string.h>
#include "kernel/calls.h"
#include "kernel/cpu.h"
#include "kernel/guest.h"

#define PRCTL_SET_KEEPCAPS_ 8
#define PRCTL_SET_NAME_ 15

#define ARCH_SET_GS_ 0x1001
#define ARCH_SET_FS_ 0x1002
#define ARCH_GET_FS_ 0x1003
#define ARCH_GET_GS_ 0x1004

int_t sys_prctl(dword_t option, uint_t arg2, uint_t UNUSED(arg3), uint_t UNUSED(arg4), uint_t UNUSED(arg5)) {
    switch (option) {
        case PRCTL_SET_KEEPCAPS_:
            // stub
            return 0;
        case PRCTL_SET_NAME_: {
            char name[16];
            if (user_read_string(arg2, name, sizeof(name) - 1))
                return _EFAULT;
            name[sizeof(name) - 1] = '\0';
            STRACE("prctl(PRCTL_SET_NAME, \"%s\")", name);
            strcpy(current->comm, name);
            return 0;
        }
        default:
            STRACE("prctl(%#x)", option);
            return _EINVAL;
    }
}

int_t sys_arch_prctl(int_t code, addr_t addr) {
    const struct guest_abi *abi = guest_abi_info(current->mm->arch);
    STRACE("arch_prctl(%#x, %#llx)", code, (unsigned long long) addr);
    if (!abi->has_arch_prctl)
        return _EINVAL;
    switch (code) {
        case ARCH_SET_GS_:
            task_cpu_set_gs_base(current, addr);
            return 0;
        case ARCH_SET_FS_:
            task_cpu_set_fs_base(current, addr);
            return 0;
        case ARCH_GET_FS_: {
            addr_t fsbase = task_cpu_fs_base(current);
            if (user_put(addr, fsbase))
                return _EFAULT;
            return 0;
        }
        case ARCH_GET_GS_: {
            addr_t gsbase = task_cpu_gs_base(current);
            if (user_put(addr, gsbase))
                return _EFAULT;
            return 0;
        }
        default:
            return _EINVAL;
    }
}

#define REBOOT_MAGIC1 0xfee1dead
#define REBOOT_MAGIC2 672274793
#define REBOOT_MAGIC2A 85072278
#define REBOOT_MAGIC2B 369367448
#define REBOOT_MAGIC2C 537993216

#define REBOOT_CMD_CAD_OFF 0
#define REBOOT_CMD_CAD_ON 0x89abcdef

int_t sys_reboot(int_t magic, int_t magic2, int_t cmd) {
    STRACE("reboot(%#x, %d, %d)", magic, magic2, cmd);
    if (!superuser())
        return _EPERM;
    if (magic != (int) REBOOT_MAGIC1 ||
            (magic2 != REBOOT_MAGIC2 &&
             magic2 != REBOOT_MAGIC2A &&
             magic2 != REBOOT_MAGIC2B &&
             magic2 != REBOOT_MAGIC2C))
        return _EINVAL;

    switch (cmd) {
        case REBOOT_CMD_CAD_ON:
        case REBOOT_CMD_CAD_OFF:
            return 0;
        default:
            return _EPERM;
    }
}
