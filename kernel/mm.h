#ifndef KERNEL_MM_H
#define KERNEL_MM_H

#include "kernel/memory.h"
#include "kernel/guest.h"
#include "misc.h"

// uses mem.lock instead of having a lock of its own
struct mm {
    atomic_uint refcount;
    struct mem mem;
    enum guest_arch arch;

    addr_t vdso; // immutable
    addr_t start_brk; // immutable
    addr_t brk;

    // crap for procfs
    addr_t argv_start;
    addr_t argv_end;
    addr_t env_start;
    addr_t env_end;
    addr_t auxv_start;
    addr_t auxv_end;
    addr_t stack_start;
    struct fd *exefile;
};

// Create a new address space
struct mm *mm_new(void);
struct mm *mm_new_arch(enum guest_arch arch);
// Clone (COW) the address space
struct mm *mm_copy(struct mm *mm);
page_t mm_find_hole(struct mm *mm, pages_t size);
// Increment the refcount
void mm_retain(struct mm *mem);
// Decrement the refcount, destroy everything in the space if 0
void mm_release(struct mm *mem);

static inline const struct guest_abi *mm_guest_abi(struct mm *mm) {
    return guest_abi_info(mm->arch);
}

#endif
