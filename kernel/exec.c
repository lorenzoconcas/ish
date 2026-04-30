#include "kernel/signal.h"
#include "task.h"
#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "misc.h"
#include "kernel/calls.h"
#include "kernel/cpu.h"
#include "kernel/random.h"
#include "kernel/errno.h"
#include "fs/fd.h"
#include "kernel/elf.h"
#include "kernel/vdso.h"
#include "tools/ptraceomatic-config.h"

#define ARGV_MAX 32 * PAGE_SIZE

struct exec_args {
    // number of arguments
    size_t count;
    // series of count null-terminated strings, plus an extra null for good measure
    const char *args;
};

struct aux_value_ent {
    uint32_t type;
    addr_t value;
};

static inline addr_t align_stack(addr_t sp);
static inline ssize_t user_strlen(addr_t p);
static inline int user_memset(addr_t start, byte_t val, dword_t len);
static inline addr_t copy_string(addr_t sp, const char *string);
static inline addr_t args_copy(addr_t sp, struct exec_args args);
static inline int stack_put_word(addr_t addr, addr_t value, uint8_t word_size);
static inline int stack_put_aux(addr_t addr, struct aux_value_ent aux, uint8_t word_size);
static size_t args_size(struct exec_args args);

static int read_exact(struct fd *fd, void *buf, size_t size) {
    ssize_t err = fd->ops->read(fd, buf, size);
    if (err != (ssize_t) size) {
        if (err < 0)
            return _EIO;
        return _ENOEXEC;
    }
    return 0;
}

static bool elf_value_fits_addr(qword_t value) {
    return value <= UINT64_MAX;
}

static bool elf_range_fits_addr(qword_t start, qword_t size) {
    qword_t end;
    return !__builtin_add_overflow(start, size, &end);
}

static bool elf_value_fits_size(qword_t value) {
    return value <= SIZE_MAX;
}

static bool elf_value_fits_offset(qword_t value) {
    return value <= INT64_MAX;
}

static int read_header(struct fd *fd, const struct guest_abi *abi, struct elf_info *header) {
    int err;
    struct elf_ident ident;

    if (fd->ops->lseek(fd, 0, SEEK_SET) < 0)
        return _EIO;
    if ((err = read_exact(fd, &ident, sizeof(ident))) < 0)
        return err;
    if (memcmp(&ident.magic, ELF_MAGIC, sizeof(ident.magic)) != 0
            || ident.bitness != abi->elf_class
            || ident.endian != ELF_LITTLEENDIAN
            || ident.elfversion1 != 1)
        return _ENOEXEC;

    if (fd->ops->lseek(fd, 0, SEEK_SET) < 0)
        return _EIO;
    if (ident.bitness == ELF_32BIT) {
        struct elf_header raw;
        if ((err = read_exact(fd, &raw, sizeof(raw))) < 0)
            return err;
        if ((raw.type != ELF_EXECUTABLE && raw.type != ELF_DYNAMIC)
                || raw.machine != abi->elf_machine
                || raw.phent_size != sizeof(struct prg_header))
            return _ENOEXEC;
        *header = (struct elf_info) {
            .bitness = raw.bitness,
            .type = raw.type,
            .machine = raw.machine,
            .entry_point = raw.entry_point,
            .prghead_off = raw.prghead_off,
            .phent_size = raw.phent_size,
            .phent_count = raw.phent_count,
        };
        return 0;
    }
    if (ident.bitness == ELF_64BIT) {
        struct elf64_header raw;
        if ((err = read_exact(fd, &raw, sizeof(raw))) < 0)
            return err;
        if ((raw.type != ELF_EXECUTABLE && raw.type != ELF_DYNAMIC)
                || raw.machine != abi->elf_machine
                || raw.phent_size != sizeof(struct prg64_header))
            return _ENOEXEC;
        *header = (struct elf_info) {
            .bitness = raw.bitness,
            .type = raw.type,
            .machine = raw.machine,
            .entry_point = raw.entry_point,
            .prghead_off = raw.prghead_off,
            .phent_size = raw.phent_size,
            .phent_count = raw.phent_count,
        };
        return 0;
    }
    return _ENOEXEC;
}

static int read_prg_headers(struct fd *fd, struct elf_info header, struct elf_prg_info **ph_out) {
    int err;

    if (header.phent_count == 0) {
        *ph_out = NULL;
        return 0;
    }
    if (!elf_value_fits_offset(header.prghead_off))
        return _ENOEXEC;

    struct elf_prg_info *ph = calloc(header.phent_count, sizeof(struct elf_prg_info));
    if (ph == NULL)
        return _ENOMEM;

    if (fd->ops->lseek(fd, (off_t_) header.prghead_off, SEEK_SET) < 0) {
        free(ph);
        return _EIO;
    }
    if (header.bitness == ELF_32BIT) {
        size_t raw_size;
        if (__builtin_mul_overflow((size_t) header.phent_count, sizeof(struct prg_header), &raw_size)) {
            free(ph);
            return _ENOMEM;
        }
        struct prg_header *raw = malloc(raw_size);
        if (raw == NULL) {
            free(ph);
            return _ENOMEM;
        }
        err = read_exact(fd, raw, raw_size);
        if (err < 0) {
            free(raw);
            free(ph);
            return err;
        }
        for (unsigned i = 0; i < header.phent_count; i++) {
            ph[i] = (struct elf_prg_info) {
                .type = raw[i].type,
                .flags = raw[i].flags,
                .offset = raw[i].offset,
                .vaddr = raw[i].vaddr,
                .paddr = raw[i].paddr,
                .filesize = raw[i].filesize,
                .memsize = raw[i].memsize,
                .alignment = raw[i].alignment,
            };
        }
        free(raw);
    } else if (header.bitness == ELF_64BIT) {
        size_t raw_size;
        if (__builtin_mul_overflow((size_t) header.phent_count, sizeof(struct prg64_header), &raw_size)) {
            free(ph);
            return _ENOMEM;
        }
        struct prg64_header *raw = malloc(raw_size);
        if (raw == NULL) {
            free(ph);
            return _ENOMEM;
        }
        err = read_exact(fd, raw, raw_size);
        if (err < 0) {
            free(raw);
            free(ph);
            return err;
        }
        for (unsigned i = 0; i < header.phent_count; i++) {
            ph[i] = (struct elf_prg_info) {
                .type = raw[i].type,
                .flags = raw[i].flags,
                .offset = raw[i].offset,
                .vaddr = raw[i].vaddr,
                .paddr = raw[i].paddr,
                .filesize = raw[i].filesize,
                .memsize = raw[i].memsize,
                .alignment = raw[i].alignment,
            };
        }
        free(raw);
    } else {
        free(ph);
        return _ENOEXEC;
    }

    *ph_out = ph;
    return 0;
}

static int load_entry(struct elf_prg_info ph, addr_t bias, struct fd *fd) {
    int err;

    if (ph.memsize < ph.filesize
            || !elf_value_fits_addr(ph.vaddr)
            || !elf_value_fits_addr(ph.filesize)
            || !elf_value_fits_addr(ph.memsize)
            || !elf_value_fits_offset(ph.offset))
        return _ENOEXEC;

    qword_t addr64;
    if (__builtin_add_overflow((qword_t) bias, ph.vaddr, &addr64)
            || !elf_range_fits_addr(addr64, ph.filesize)
            || !elf_range_fits_addr(addr64, ph.memsize))
        return _ENOEXEC;

    addr_t addr = addr64;
    off_t_ offset = ph.offset;
    addr_t memsize = ph.memsize;
    addr_t filesize = ph.filesize;

    int flags = P_READ;
    if (ph.flags & PH_W) flags |= P_WRITE;

    if ((err = fd->ops->mmap(fd, current->mem, PAGE(addr),
                    PAGE_ROUND_UP(filesize + PGOFFSET(addr)),
                    offset - PGOFFSET(addr), flags, MMAP_PRIVATE)) < 0)
        return err;
    // TODO find a better place for these to avoid code duplication
    mem_pt(current->mem, PAGE(addr))->data->fd = fd_retain(fd);
    mem_pt(current->mem, PAGE(addr))->data->file_offset = offset - PGOFFSET(addr);

    if (memsize > filesize) {
        // put zeroes between addr + filesize and addr + memsize, call that bss
        dword_t bss_size = memsize - filesize;

        // first zero the tail from the end of the file mapping to the end
        // of the load entry or the end of the page, whichever comes first
        addr_t file_end = addr + filesize;
        dword_t tail_size = PAGE_SIZE - PGOFFSET(file_end);
        if (tail_size == PAGE_SIZE)
            // if you can calculate tail_size better and not have to do this please let me know
            tail_size = 0;
        if (tail_size > bss_size)
            tail_size = bss_size;

        if (tail_size != 0) {
            // Unlock and lock the mem because the user functions must be
            // called without locking mem.
            write_wrunlock(&current->mem->lock);
            user_memset(file_end, 0, tail_size);
            write_wrlock(&current->mem->lock);
        }

        // then map the pages from after the file mapping up to and including the end of bss
        if (bss_size - tail_size != 0)
            if ((err = pt_map_nothing(current->mem, PAGE_ROUND_UP(addr + filesize),
                    PAGE_ROUND_UP(bss_size - tail_size), flags)) < 0)
                return err;
    }
    return 0;
}

static int find_hole_for_elf(struct elf_info *header, struct elf_prg_info *ph, addr_t *hole_out) {
    bool found = false;
    qword_t low = 0;
    qword_t high = 0;
    for (unsigned i = 0; i < header->phent_count; i++) {
        if (ph[i].type != PT_LOAD)
            continue;
        qword_t end;
        if (__builtin_add_overflow(ph[i].vaddr, ph[i].memsize, &end))
            return _ENOEXEC;
        qword_t seg_low = PAGE(ph[i].vaddr);
        qword_t seg_high = PAGE_ROUND_UP(end);
        if (!found || seg_low < low)
            low = seg_low;
        if (!found || seg_high > high)
            high = seg_high;
        found = true;
    }
    if (!found) {
        *hole_out = 0;
        return 0;
    }
    if (high < low || high - low > MEM_PAGES)
        return _ENOEXEC;

    page_t hole = mm_find_hole(current->mm, high - low);
    if (hole == BAD_PAGE)
        return _ENOMEM;
    if (!elf_value_fits_addr(hole << PAGE_BITS))
        return _ENOMEM;
    *hole_out = hole << PAGE_BITS;
    return 0;
}

static int elf_exec(struct fd *fd, const char *file, struct exec_args argv, struct exec_args envp) {
    int err = 0;
    enum guest_arch arch = current ? current->mm->arch : guest_default_abi()->arch;
    const struct guest_abi *abi = guest_abi_info(arch);

    // read the headers
    struct elf_info header;
    if ((err = read_header(fd, abi, &header)) < 0)
        return err;
    struct elf_prg_info *ph;
    if ((err = read_prg_headers(fd, header, &ph)) < 0)
        return err;

    // look for an interpreter
    char *interp_name = NULL;
    struct fd *interp_fd = NULL;
    struct elf_info interp_header;
    struct elf_prg_info *interp_ph = NULL;
    for (unsigned i = 0; i < header.phent_count; i++) {
        if (ph[i].type != PT_INTERP)
            continue;
        if (interp_name) {
            // can't have two interpreters
            err = _EINVAL;
            goto out_free_interp;
        }

        if (ph[i].filesize == 0 || !elf_value_fits_size(ph[i].filesize)
                || !elf_value_fits_offset(ph[i].offset)) {
            err = _ENOEXEC;
            goto out_free_interp;
        }

        size_t interp_name_size = ph[i].filesize;
        interp_name = malloc(interp_name_size);
        err = _ENOMEM;
        if (interp_name == NULL)
            goto out_free_ph;

        // read the interpreter name out of the file
        if (fd->ops->lseek(fd, (off_t_) ph[i].offset, SEEK_SET) < 0)
            goto out_free_interp;
        if ((err = read_exact(fd, interp_name, interp_name_size)) < 0)
            goto out_free_interp;
        if (interp_name[interp_name_size - 1] != '\0') {
            err = _ENOEXEC;
            goto out_free_interp;
        }

        // open interpreter and read headers
        interp_fd = generic_open(interp_name, O_RDONLY, 0);
        if (IS_ERR(interp_fd)) {
            err = PTR_ERR(interp_fd);
            goto out_free_interp;
        }
        if ((err = read_header(interp_fd, abi, &interp_header)) < 0) {
            if (err == _ENOEXEC) err = _ELIBBAD;
            goto out_free_interp;
        }
        if ((err = read_prg_headers(interp_fd, interp_header, &interp_ph)) < 0) {
            if (err == _ENOEXEC) err = _ELIBBAD;
            goto out_free_interp;
        }
    }

    // x86_64 dynamic startup is still experimental: there is no 64-bit VDSO
    // yet, so auxv exposes no VDSO entry for this ABI. Let the interpreter run
    // anyway so the remaining blockers surface as concrete missing syscalls or
    // instructions instead of an unconditional exec-time rejection.

    // free the process's memory.
    // from this point on, if any error occurs the process will have to be
    // killed before it even starts. please don't be too sad about it, it's
    // just a process.
    //
    // general_lock protects current->mm. otherwise procfs might read the
    // pointer before it's released and then try to lock it after it's
    // released.
    lock(&current->general_lock);
    mm_release(current->mm);
    task_set_mm(current, mm_new_arch(arch));
    current->mm->arch = abi->arch;
    unlock(&current->general_lock);
    write_wrlock(&current->mem->lock);

    current->mm->exefile = fd_retain(fd);

    addr_t load_addr = 0; // used for AX_PHDR
    bool load_addr_set = false;
    addr_t bias = 0; // offset for loading shared libraries as executables

    // map dat shit!
    for (unsigned i = 0; i < header.phent_count; i++) {
        if (ph[i].type != PT_LOAD)
            continue;

        if (!load_addr_set && header.type == ELF_DYNAMIC) {
            // see giant comment in linux/fs/binfmt_elf.c, around line 950
            if (interp_name)
                bias = 0x56555000; // I have no idea how this number was arrived at
            else if ((err = find_hole_for_elf(&header, ph, &bias)) < 0)
                goto beyond_hope;
        }

        if ((err = load_entry(ph[i], bias, fd)) < 0)
            goto beyond_hope;

        // load_addr is used to get a value for AX_PHDR et al
        if (!load_addr_set) {
            qword_t load_addr64;
            if (ph[i].offset > ph[i].vaddr
                    || __builtin_add_overflow((qword_t) bias, ph[i].vaddr - ph[i].offset, &load_addr64)
                    || !elf_value_fits_addr(load_addr64)) {
                err = _ENOEXEC;
                goto beyond_hope;
            }
            load_addr = load_addr64;
            load_addr_set = true;
        }

        // we have to know where the brk starts
        qword_t brk64;
        if (__builtin_add_overflow((qword_t) bias, ph[i].vaddr, &brk64)
                || __builtin_add_overflow(brk64, ph[i].memsize, &brk64)
                || !elf_value_fits_addr(brk64)) {
            err = _ENOEXEC;
            goto beyond_hope;
        }
        addr_t brk = brk64;
        if (brk > current->mm->start_brk)
            current->mm->start_brk = current->mm->brk = BYTES_ROUND_UP(brk);
    }

    if (!load_addr_set) {
        err = _ENOEXEC;
        goto beyond_hope;
    }

    qword_t entry64;
    if (__builtin_add_overflow((qword_t) bias, header.entry_point, &entry64)
            || !elf_value_fits_addr(entry64)) {
        err = _ENOEXEC;
        goto beyond_hope;
    }
    addr_t program_entry = entry64;
    addr_t entry = program_entry;
    addr_t interp_base = 0;

    if (interp_name) {
        // map dat shit! interpreter edition
        if ((err = find_hole_for_elf(&interp_header, interp_ph, &interp_base)) < 0)
            goto beyond_hope;
        for (int i = (int) interp_header.phent_count - 1; i >= 0; i--) {
            if (interp_ph[i].type != PT_LOAD)
                continue;
            if ((err = load_entry(interp_ph[i], interp_base, interp_fd)) < 0)
                goto beyond_hope;
        }
        if (__builtin_add_overflow((qword_t) interp_base, interp_header.entry_point, &entry64)
                || !elf_value_fits_addr(entry64)) {
            err = _ENOEXEC;
            goto beyond_hope;
        }
        entry = entry64;
    }

    current->mm->vdso = 0;
    addr_t vdso_entry = 0;
    if (abi->word_size == sizeof(dword_t)) {
        // map vdso
        err = _ENOMEM;
        pages_t vdso_pages = sizeof(vdso_data) >> PAGE_BITS;
        // FIXME disgusting hack: musl's dynamic linker has a one-page hole, and
        // I'd rather not put the vdso in that hole. so find a two-page hole and
        // add one.
        page_t vdso_page = mm_find_hole(current->mm, vdso_pages + 1);
        if (vdso_page == BAD_PAGE)
            goto beyond_hope;
        vdso_page += 1;
        if ((err = pt_map(current->mem, vdso_page, vdso_pages, (void *) vdso_data, 0, 0)) < 0)
            goto beyond_hope;
        mem_pt(current->mem, vdso_page)->data->name = "[vdso]";
        current->mm->vdso = vdso_page << PAGE_BITS;
        vdso_entry = current->mm->vdso + ((struct elf_header *) vdso_data)->entry_point;

        // map 3 empty "vvar" pages to satisfy ptraceomatic
        page_t vvar_page = mm_find_hole(current->mm, VVAR_PAGES);
        if (vvar_page == BAD_PAGE)
            goto beyond_hope;
        if ((err = pt_map_nothing(current->mem, vvar_page, VVAR_PAGES, 0)) < 0)
            goto beyond_hope;
        mem_pt(current->mem, vvar_page)->data->name = "[vvar]";
    }

    // STACK TIME!

    // allocate the initial stack page and let it grow down
    if ((err = pt_map_nothing(current->mem, abi->stack_page, 1, P_WRITE | P_GROWSDOWN)) < 0)
        goto beyond_hope;
    // that was the last memory mapping
    write_wrunlock(&current->mem->lock);
    addr_t sp = abi->stack_top;
    // Linux leaves a word-sized hole at the bottom of the initial stack.
    sp -= abi->stack_spare_bytes;

    err = _EFAULT;
    // first, copy stuff pointed to by argv/envp/auxv
    // filename, argc, argv
    addr_t file_addr = sp = copy_string(sp, file);
    if (sp == 0)
        goto beyond_hope;
    addr_t envp_addr = sp = args_copy(sp, envp);
    if (sp == 0)
        goto beyond_hope;
    current->mm->env_start = envp_addr;
    current->mm->env_end = file_addr;
    current->mm->argv_end = sp;
    addr_t argv_addr = sp = args_copy(sp, argv);
    if (sp == 0)
        goto beyond_hope;
    current->mm->argv_start = sp;
    sp = align_stack(sp);

    addr_t platform_addr = sp = copy_string(sp, abi->platform);
    if (sp == 0)
        goto beyond_hope;
    // 16 random bytes so no system call is needed to seed a userspace RNG
    char random[16] = {};
    get_random(random, sizeof(random)); // if this fails, eh, no one's really using it
    addr_t random_addr = sp -= sizeof(random);
    if (user_put(sp, random))
        goto beyond_hope;

    // the way linux aligns the stack at this point is kinda funky
    // calculate how much space is needed for argv, envp, and auxv, subtract
    // that from sp, then align, then copy argv/envp/auxv from that down

    // declare elf aux now so we can know how big it is
    qword_t phdr64;
    if (__builtin_add_overflow((qword_t) load_addr, header.prghead_off, &phdr64)
            || !elf_value_fits_addr(phdr64)) {
        err = _ENOEXEC;
        goto beyond_hope;
    }
    struct aux_value_ent aux[] = {
        {AX_SYSINFO, vdso_entry},
        {AX_SYSINFO_EHDR, current->mm->vdso},
        {AX_HWCAP, 0x00000000}, // suck that
        {AX_PAGESZ, PAGE_SIZE},
        {AX_CLKTCK, 0x64},
        {AX_PHDR, phdr64},
        {AX_PHENT, header.phent_size},
        {AX_PHNUM, header.phent_count},
        {AX_BASE, interp_base},
        {AX_FLAGS, 0},
        {AX_ENTRY, program_entry},
        {AX_UID, 0},
        {AX_EUID, 0},
        {AX_GID, 0},
        {AX_EGID, 0},
        {AX_SECURE, 0},
        {AX_RANDOM, random_addr},
        {AX_HWCAP2, 0}, // suck that too
        {AX_EXECFN, file_addr},
        {AX_PLATFORM, platform_addr},
        {0, 0}
    };
    size_t aux_bytes;
    if (__builtin_mul_overflow(array_size(aux), (size_t) abi->word_size * 2, &aux_bytes)) {
        err = _ENOMEM;
        goto beyond_hope;
    }
    sp -= ((argv.count + 1) + (envp.count + 1) + 1) * abi->word_size;
    sp -= aux_bytes;
    sp &=~ 0xf;

    // now copy down, start using p so sp is preserved
    addr_t p = sp;

    // argc
    if (stack_put_word(p, argv.count, abi->word_size))
        return _EFAULT;
    p += abi->word_size;

    // argv
    size_t argc = argv.count;
    while (argc-- > 0) {
        if (stack_put_word(p, argv_addr, abi->word_size))
            return _EFAULT;
        argv_addr += user_strlen(argv_addr) + 1;
        p += abi->word_size;
    }
    if (stack_put_word(p, 0, abi->word_size))
        return _EFAULT;
    p += abi->word_size;

    // envp
    size_t envc = envp.count;
    while (envc-- > 0) {
        if (stack_put_word(p, envp_addr, abi->word_size))
            return _EFAULT;
        envp_addr += user_strlen(envp_addr) + 1;
        p += abi->word_size;
    }
    if (stack_put_word(p, 0, abi->word_size))
        return _EFAULT;
    p += abi->word_size;

    // copy auxv
    current->mm->auxv_start = p;
    for (size_t i = 0; i < array_size(aux); i++) {
        if (stack_put_aux(p, aux[i], abi->word_size))
            goto beyond_hope;
        p += abi->word_size * 2;
    }
    current->mm->auxv_end = p;

    current->mm->stack_start = sp;
    task_cpu_reset_exec_state(current, sp, entry);
    current->cpu.fcw = 0x37f;

    // This code was written when I discovered that the glibc entry point
    // interprets edx as the address of a function to call on exit, as
    // specified in the ABI. This register is normally set by the dynamic
    // linker, so everything works fine until you run a static executable.
    collapse_flags(&current->cpu);
    current->cpu.eflags = 0;

    err = 0;
out_free_interp:
    if (interp_name != NULL)
        free(interp_name);
    if (interp_fd != NULL && !IS_ERR(interp_fd))
        fd_close(interp_fd);
    if (interp_ph != NULL)
        free(interp_ph);
out_free_ph:
    free(ph);
    return err;

beyond_hope:
    // TODO force sigsegv
    write_wrunlock(&current->mem->lock);
    goto out_free_interp;
}

static size_t args_size(struct exec_args args) {
    const char *args_end = args.args;
    for (size_t i = 0; i < args.count; i++) {
        args_end += strlen(args_end) + 1;
    }
    // don't forget the very last null terminator
    assert(args_end[0] == '\0');
    args_end++;
    return args_end - args.args;
}

static inline addr_t align_stack(addr_t sp) {
    return sp &~ 0xf;
}

static inline addr_t copy_string(addr_t sp, const char *string) {
    sp -= strlen(string) + 1;
    if (user_write_string(sp, string))
        return 0;
    return sp;
}

static inline addr_t args_copy(addr_t sp, struct exec_args args) {
    size_t size = args_size(args);
    sp -= size;
    if (user_write(sp, args.args, size))
        return 0;
    return sp;
}

static inline int stack_put_word(addr_t addr, addr_t value, uint8_t word_size) {
    if (word_size == sizeof(dword_t)) {
        if (value > UINT32_MAX)
            return 1;
        dword_t value32 = value;
        return user_put(addr, value32);
    }
    qword_t value64 = value;
    return user_put(addr, value64);
}

static inline int stack_put_aux(addr_t addr, struct aux_value_ent aux, uint8_t word_size) {
    if (stack_put_word(addr, aux.type, word_size))
        return 1;
    return stack_put_word(addr + word_size, aux.value, word_size);
}

static inline ssize_t user_strlen(addr_t p) {
    size_t i = 0;
    char c;
    do {
        if (user_get(p + i, c))
            return -1;
        i++;
    } while (c != '\0');
    return i - 1;
}

static inline int user_memset(addr_t start, byte_t val, dword_t len) {
    while (len--)
        if (user_put(start++, val))
            return 1;
    return 0;
}

static int format_exec(struct fd *fd, const char *file, struct exec_args argv, struct exec_args envp) {
    int err = elf_exec(fd, file, argv, envp);
    if (err != _ENOEXEC)
        return err;
    // other formats would go here
    return _ENOEXEC;
}

static int shebang_exec(struct fd *fd, const char *file, struct exec_args argv, struct exec_args envp) {
    // read the first 128 bytes to get the shebang line out of
    if (fd->ops->lseek(fd, 0, SEEK_SET))
        return _EIO;
    char header[128];
    int size = fd->ops->read(fd, header, sizeof(header) - 1);
    if (size < 0)
        return _EIO;
    header[size] = '\0';

    // only look at the first line
    char *newline = strchr(header, '\n');
    if (newline == NULL)
        return _ENOEXEC;
    *newline = '\0';

    // format: #![spaces]interpreter[spaces]argument[spaces]
    char *p = header;
    if (p[0] != '#' || p[1] != '!')
        return _ENOEXEC;
    p += 2;
    while (*p == ' ')
        p++;
    if (*p == '\0')
        return _ENOEXEC;

    char *interpreter = p;
    while (*p != ' ' && *p != '\0')
        p++;
    if (*p != '\0') {
        *p++ = '\0';
        while (*p == ' ')
            p++;
    }

    char *argument = p;
    // strip trailing whitespace
    p = strchr(p, '\0') - 1;
    while (*p == ' ')
        *p-- = '\0';
    if (*argument == '\0')
        argument = NULL;

    struct exec_args argv_rest = {
        .count = argv.count - 1,
        .args = argv.args + strlen(argv.args) + 1,
    };
    size_t args_rest_size = args_size(argv_rest);
    size_t extra_args_size = strlen(interpreter) + 1 + strlen(file) + 1;
    if (argument)
        extra_args_size += strlen(argument) + 1;
    if (args_rest_size + extra_args_size >= ARGV_MAX)
        return _E2BIG;

    char new_argv_buf[ARGV_MAX];
    struct exec_args new_argv = {.args = new_argv_buf};
    size_t n = 0;
    strcpy(new_argv_buf, interpreter);
    new_argv.count++;
    n += strlen(interpreter) + 1;
    if (argument) {
        strcpy(new_argv_buf + n, argument);
        new_argv.count++;
        n += strlen(argument) + 1;
    }
    strcpy(new_argv_buf + n, file);
    n += strlen(file) + 1;
    new_argv.count++;
    memcpy(new_argv_buf + n, argv_rest.args, args_rest_size);
    new_argv.count += argv_rest.count;

    struct fd *interpreter_fd = generic_open(interpreter, O_RDONLY_, 0);
    if (IS_ERR(interpreter_fd))
        return PTR_ERR(interpreter_fd);
    int err = format_exec(interpreter_fd, interpreter, new_argv, envp);
    fd_close(interpreter_fd);
    return err;
}

int __do_execve(const char *file, struct exec_args argv, struct exec_args envp) {
    struct fd *fd = generic_open(file, O_RDONLY, 0);
    if (IS_ERR(fd))
        return PTR_ERR(fd);

    struct statbuf stat;
    int err = fd->mount->fs->fstat(fd, &stat);
    if (err < 0) {
        fd_close(fd);
        return err;
    }

    // if nobody has permission to execute, it should be safe to not execute
    if (!(stat.mode & 0111)) {
        fd_close(fd);
        return _EACCES;
    }

    err = format_exec(fd, file, argv, envp);
    if (err == _ENOEXEC)
        err = shebang_exec(fd, file, argv, envp);
    fd_close(fd);
    if (err < 0)
        return err;

    // setuid/setgid
    if (stat.mode & S_ISUID) {
        current->suid = current->euid;
        current->euid = stat.uid;
    }
    if (stat.mode & S_ISGID) {
        current->sgid = current->egid;
        current->egid = stat.gid;
    }

    // save current->comm
    lock(&current->general_lock);
    const char *basename = strrchr(file, '/');
    if (basename == NULL)
        basename = file;
    else
        basename++;
    strncpy(current->comm, basename, sizeof(current->comm));
    unlock(&current->general_lock);

    update_thread_name();

    // cloexec
    // consider putting this in fd.c?
    fdtable_do_cloexec(current->files);

    // reset signal handlers
    lock(&current->sighand->lock);
    for (int sig = 0; sig < NUM_SIGS; sig++) {
        struct sigaction_ *action = &current->sighand->action[sig];
        if (action->handler != SIG_IGN_)
            action->handler = SIG_DFL_;
    }
    current->sighand->altstack = 0;
    unlock(&current->sighand->lock);

    current->did_exec = true;
    vfork_notify(current);

    if (current->ptrace.traced) {
        lock(&pids_lock);
        send_signal(current, SIGTRAP_, (struct siginfo_) {
            .code = SI_USER_,
            .kill.pid = current->pid,
            .kill.uid = current->uid,
        });
        unlock(&pids_lock);
    }

    return 0;
}

int do_execve(const char *file, size_t argc, const char *argv_p, const char *envp_p) {
    struct exec_args argv = {.count = argc, .args = argv_p};
    struct exec_args envp = {.args = envp_p};
    while (*envp_p != '\0') {
        envp_p += strlen(envp_p) + 1;
        envp.count++;
    }
    return __do_execve(file, argv, envp);
}

static ssize_t user_read_string_array(addr_t addr, char *buf, size_t max) {
    size_t i = 0;
    size_t p = 0;
    for (;;) {
        addr_t str_addr;
        if (user_get(addr + i * sizeof(addr_t), str_addr))
            return _EFAULT;
        if (str_addr == 0)
            break;
        size_t str_p = 0;
        for (;;) {
            if (p >= max)
                return _E2BIG;
            if (user_get(str_addr + str_p, buf[p]))
                return _EFAULT;
            str_p++;
            p++;
            if (buf[p - 1] == '\0')
                break;
        }
        i++;
    }
    if (p >= max)
        return _E2BIG;
    buf[p] = '\0';
    return i;
}

dword_t sys_execve(addr_t filename_addr, addr_t argv_addr, addr_t envp_addr) {
    char filename[MAX_PATH];
    if (user_read_string(filename_addr, filename, sizeof(filename)))
        return _EFAULT;

    int err = _ENOMEM;
    char *argv = malloc(ARGV_MAX);
    if (argv == NULL)
        goto err_free_argv;
    ssize_t argc = user_read_string_array(argv_addr, argv, ARGV_MAX);
    if (argc < 0) {
        err = argc;
        goto err_free_argv;
    }

    char *envp = malloc(ARGV_MAX);
    if (envp == NULL)
        goto err_free_envp;
    if (envp_addr != 0) {
        err = user_read_string_array(envp_addr, envp, ARGV_MAX);
        if (err < 0)
            goto err_free_envp;
    } else {
        // Do not take advantage of this nonstandard and nonportable misfeature!
        // - Michael Kerrisk, execve(2)
        envp[0] = envp[1] = '\0';
    }

    STRACE("execve(\"%.1000s\", {", filename);
    const char *args = argv;
    while (*args != '\0') {
        STRACE("\"%.1000s\", ", args);
        args += strlen(args) + 1;
    }
    STRACE("}, {");
    args = envp;
    while (*args != '\0') {
        STRACE("\"%.1000s\", ", args);
        args += strlen(args) + 1;
    }
    STRACE("})");

    err = do_execve(filename, argc, argv, envp);

err_free_envp:
    free(envp);
err_free_argv:
    free(argv);
    return err;
}
