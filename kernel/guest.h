#ifndef KERNEL_GUEST_H
#define KERNEL_GUEST_H

#include <string.h>
#include "emu/mmu.h"
#include "kernel/elf.h"

enum guest_arch {
    GUEST_ARCH_X86_32,
    GUEST_ARCH_X86_64,
};

struct guest_abi {
    enum guest_arch arch;
    const char *name;
    uint8_t elf_class;
    uint16_t elf_machine;
    const char *platform;
    uint8_t word_size;
    uint8_t stack_spare_bytes;
    bool has_arch_prctl;
    page_t mmap_hole_low_page;
    page_t mmap_hole_high_page;
    page_t stack_page;
    addr_t stack_top;
};

enum guest_arch default_guest_arch(void);
void set_default_guest_arch(enum guest_arch arch);

static inline const struct guest_abi *guest_abi_info(enum guest_arch arch) {
    static const struct guest_abi guest_abi_x86_32 = {
        .arch = GUEST_ARCH_X86_32,
        .name = "x86_32",
        .elf_class = ELF_32BIT,
        .elf_machine = ELF_X86,
        .platform = "i686",
        .word_size = 4,
        .stack_spare_bytes = 4,
        .has_arch_prctl = false,
        .mmap_hole_low_page = 0x40000,
        .mmap_hole_high_page = 0xf7ffd,
        .stack_page = 0xffffd,
        .stack_top = 0xffffe000,
    };
    static const struct guest_abi guest_abi_x86_64 = {
        .arch = GUEST_ARCH_X86_64,
        .name = "x86_64",
        .elf_class = ELF_64BIT,
        .elf_machine = ELF_X86_64,
        .platform = "x86_64",
        .word_size = 8,
        .stack_spare_bytes = 8,
        .has_arch_prctl = true,
        // These stay conservative placeholders until the address-space port
        // widens guest pointers beyond 32 bits.
        .mmap_hole_low_page = 0x40000,
        .mmap_hole_high_page = 0xf7ffd,
        .stack_page = 0xffffd,
        .stack_top = 0xffffe000,
    };

    switch (arch) {
        case GUEST_ARCH_X86_32:
            return &guest_abi_x86_32;
        case GUEST_ARCH_X86_64:
            return &guest_abi_x86_64;
        default:
            return &guest_abi_x86_32;
    }
}

static inline const char *guest_arch_name(enum guest_arch arch) {
    return guest_abi_info(arch)->name;
}

static inline bool guest_arch_parse(const char *name, enum guest_arch *arch_out) {
    if (strcmp(name, "x86") == 0 || strcmp(name, "x86_32") == 0 || strcmp(name, "i386") == 0 || strcmp(name, "i686") == 0) {
        *arch_out = GUEST_ARCH_X86_32;
        return true;
    }
    if (strcmp(name, "x86_64") == 0 || strcmp(name, "amd64") == 0) {
        *arch_out = GUEST_ARCH_X86_64;
        return true;
    }
    return false;
}

static inline const struct guest_abi *guest_default_abi(void) {
    return guest_abi_info(default_guest_arch());
}

#endif
