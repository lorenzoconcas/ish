#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define DEFAULT_CHANNEL memory
#include "debug.h"
#include "kernel/errno.h"
#include "kernel/signal.h"
#include "kernel/memory.h"
#include "asbestos/asbestos.h"
#include "kernel/vdso.h"
#include "kernel/task.h"
#include "fs/fd.h"

// increment the change count
static void mem_changed(struct mem *mem);
static struct mmu_ops mem_mmu_ops;
static void mem_data_release(struct data *data);

struct pt_dir {
    page_t base;
    size_t used;
    struct list chain;
    struct pt_entry entries[MEM_PGDIR_SIZE];
};

#define PGDIR_TOP(page) ((page) & ~(((page_t) MEM_PGDIR_SIZE) - 1))
#define PGDIR_BOTTOM(page) ((page) & (MEM_PGDIR_SIZE - 1))

static struct pt_dir *mem_find_pgdir(struct mem *mem, page_t page) {
    page_t base = PGDIR_TOP(page);
    if (mem->pt_dir_cache != NULL && mem->pt_dir_cache->base == base)
        return mem->pt_dir_cache;

    struct list *item;
    list_for_each(&mem->pt_dirs, item) {
        struct pt_dir *pgdir = list_entry(item, struct pt_dir, chain);
        if (pgdir->base == base) {
            mem->pt_dir_cache = pgdir;
            return pgdir;
        }
        if (pgdir->base > base)
            break;
    }
    return NULL;
}

static struct pt_dir *mem_find_pgdir_ceiling(struct mem *mem, page_t page) {
    struct list *item;
    list_for_each(&mem->pt_dirs, item) {
        struct pt_dir *pgdir = list_entry(item, struct pt_dir, chain);
        if (pgdir->base >= PGDIR_TOP(page)) {
            mem->pt_dir_cache = pgdir;
            return pgdir;
        }
    }
    return NULL;
}

static struct pt_dir *mem_pgdir_new(struct mem *mem, page_t page) {
    struct pt_dir *pgdir = mem_find_pgdir(mem, page);
    if (pgdir != NULL)
        return pgdir;

    pgdir = calloc(1, sizeof(struct pt_dir));
    if (pgdir == NULL)
        return NULL;
    pgdir->base = PGDIR_TOP(page);

    struct list *before = &mem->pt_dirs;
    struct list *item;
    list_for_each(&mem->pt_dirs, item) {
        struct pt_dir *existing = list_entry(item, struct pt_dir, chain);
        if (existing->base > pgdir->base) {
            before = item;
            break;
        }
    }
    list_add_before(before, &pgdir->chain);
    mem->pt_dir_cache = pgdir;
    mem->pt_dir_count++;
    return pgdir;
}

static bool page_range_end(page_t start, pages_t pages, page_t *end_out) {
    qword_t end;
    if (__builtin_add_overflow((qword_t) start, (qword_t) pages, &end))
        return false;
    *end_out = end;
    return true;
}

static struct pt_entry *mem_pt_new(struct mem *mem, page_t page) {
    struct pt_dir *pgdir = mem_pgdir_new(mem, page);
    if (pgdir == NULL)
        return NULL;
    struct pt_entry *entry = &pgdir->entries[PGDIR_BOTTOM(page)];
    if (entry->data == NULL)
        pgdir->used++;
    return entry;
}

void mem_init(struct mem *mem) {
    list_init(&mem->pt_dirs);
    mem->pt_dir_cache = NULL;
    mem->pt_dir_count = 0;
    mem->mmu.ops = &mem_mmu_ops;
    mem->mmu.asbestos = asbestos_new(&mem->mmu);
    mem->mmu.changes = 0;
    mem->mmu.guest_word_size = 4;
    wrlock_init(&mem->lock);
}

void mem_destroy(struct mem *mem) {
    write_wrlock(&mem->lock);
    struct list *item, *tmp;
    list_for_each_safe(&mem->pt_dirs, item, tmp) {
        struct pt_dir *pgdir = list_entry(item, struct pt_dir, chain);
        for (size_t i = 0; i < MEM_PGDIR_SIZE; i++) {
            struct pt_entry *entry = &pgdir->entries[i];
            if (entry->data != NULL) {
                entry->data->refcount--;
                mem_data_release(entry->data);
            }
        }
        list_remove(&pgdir->chain);
        free(pgdir);
    }
    mem->pt_dir_cache = NULL;
    mem->pt_dir_count = 0;
    asbestos_free(mem->mmu.asbestos);
    write_wrunlock(&mem->lock);
    wrlock_destroy(&mem->lock);
}

struct pt_entry *mem_pt(struct mem *mem, page_t page) {
    struct pt_dir *pgdir = mem_find_pgdir(mem, page);
    if (pgdir == NULL)
        return NULL;
    struct pt_entry *entry = &pgdir->entries[PGDIR_BOTTOM(page)];
    if (entry->data == NULL)
        return NULL;
    return entry;
}

static void mem_pt_del(struct mem *mem, page_t page) {
    struct pt_dir *pgdir = mem_find_pgdir(mem, page);
    if (pgdir == NULL)
        return;
    struct pt_entry *entry = &pgdir->entries[PGDIR_BOTTOM(page)];
    if (entry->data == NULL)
        return;

    *entry = (struct pt_entry) {};
    pgdir->used--;
    if (pgdir->used != 0)
        return;

    if (mem->pt_dir_cache == pgdir)
        mem->pt_dir_cache = NULL;
    list_remove(&pgdir->chain);
    mem->pt_dir_count--;
    free(pgdir);
}

void mem_next_page(struct mem *mem, page_t *page) {
    if (*page == BAD_PAGE)
        return;
    (*page)++;
    struct pt_dir *pgdir = mem_find_pgdir_ceiling(mem, *page);
    if (pgdir == NULL) {
        *page = BAD_PAGE;
        return;
    }
    if (pgdir->base > PGDIR_TOP(*page))
        *page = pgdir->base;
}

page_t mem_next_mapped_page(struct mem *mem, page_t page) {
    struct pt_dir *pgdir = mem_find_pgdir_ceiling(mem, page);
    while (pgdir != NULL) {
        size_t start = pgdir->base == PGDIR_TOP(page) ? PGDIR_BOTTOM(page) : 0;
        for (size_t i = start; i < MEM_PGDIR_SIZE; i++) {
            if (pgdir->entries[i].data != NULL)
                return pgdir->base + i;
        }
        if (pgdir->chain.next == &mem->pt_dirs)
            break;
        pgdir = list_entry(pgdir->chain.next, struct pt_dir, chain);
    }
    return BAD_PAGE;
}

static page_t mem_prev_mapped_page(struct mem *mem, page_t page) {
    if (page == 0)
        return BAD_PAGE;

    page--;
    struct list *item = mem->pt_dirs.prev;
    while (item != &mem->pt_dirs) {
        struct pt_dir *pgdir = list_entry(item, struct pt_dir, chain);
        if (pgdir->base > PGDIR_TOP(page)) {
            item = item->prev;
            continue;
        }

        size_t start = pgdir->base == PGDIR_TOP(page) ? PGDIR_BOTTOM(page) : MEM_PGDIR_SIZE - 1;
        for (ssize_t i = start; i >= 0; i--) {
            if (pgdir->entries[i].data != NULL)
                return pgdir->base + i;
        }
        item = item->prev;
    }
    return BAD_PAGE;
}

page_t pt_find_hole_range(struct mem *mem, page_t low, page_t high, pages_t size) {
    if (size == 0 || high <= low)
        return BAD_PAGE;

    page_t hole_end = high + 1;
    while (hole_end > low + 1) {
        page_t mapped = mem_prev_mapped_page(mem, hole_end);
        page_t hole_start = mapped == BAD_PAGE || mapped <= low ? low + 1 : mapped + 1;
        if (hole_end - hole_start >= size)
            return hole_end - size;
        if (mapped == BAD_PAGE || mapped <= low)
            break;
        hole_end = mapped;
    }
    return BAD_PAGE;
}

page_t pt_find_hole(struct mem *mem, pages_t size) {
    return pt_find_hole_range(mem, 0x40000, 0xf7ffd, size);
}

bool pt_is_hole(struct mem *mem, page_t start, pages_t pages) {
    page_t end;
    if (!page_range_end(start, pages, &end))
        return false;
    page_t mapped = mem_next_mapped_page(mem, start);
    return mapped == BAD_PAGE || mapped >= end;
}

int pt_map(struct mem *mem, page_t start, pages_t pages, void *memory, size_t offset, unsigned flags) {
    if (memory == MAP_FAILED)
        return errno_map();
    if (pages == 0)
        return 0;
    if ((size_t) pages > (SIZE_MAX - offset) / PAGE_SIZE)
        return _ENOMEM;

    page_t end;
    if (!page_range_end(start, pages, &end))
        return _ENOMEM;

    // If this fails, the munmap in pt_unmap would probably fail.
    assert((uintptr_t) memory % real_page_size == 0 || memory == vdso_data);

    struct data *data = malloc(sizeof(struct data));
    if (data == NULL)
        return _ENOMEM;
    *data = (struct data) {
        .data = memory,
        .size = pages * PAGE_SIZE + offset,

#if LEAK_DEBUG
        .pid = current ? current->pid : 0,
        .dest = start << PAGE_BITS,
#endif
    };

    for (page_t page = start; page < end; page++) {
        if (mem_pt(mem, page) != NULL)
            pt_unmap(mem, page, 1);
        data->refcount++;
        struct pt_entry *pt = mem_pt_new(mem, page);
        if (pt == NULL) {
            data->refcount--;
            if (page != start)
                pt_unmap_always(mem, start, page - start);
            else
                mem_data_release(data);
            return _ENOMEM;
        }
        pt->data = data;
        pt->offset = ((page - start) << PAGE_BITS) + offset;
        pt->flags = flags;
    }
    return 0;
}

int pt_unmap(struct mem *mem, page_t start, pages_t pages) {
    page_t end;
    if (!page_range_end(start, pages, &end))
        return -1;
    for (page_t page = start; page < end; page++)
        if (mem_pt(mem, page) == NULL)
            return -1;
    return pt_unmap_always(mem, start, pages);
}

int pt_unmap_always(struct mem *mem, page_t start, pages_t pages) {
    page_t end;
    if (!page_range_end(start, pages, &end))
        return -1;

    for (page_t page = mem_next_mapped_page(mem, start);
            page != BAD_PAGE && page < end;
            page = mem_next_mapped_page(mem, page + 1)) {
        struct pt_entry *pt = mem_pt(mem, page);
        asbestos_invalidate_page(mem->mmu.asbestos, page);
        struct data *data = pt->data;
        mem_pt_del(mem, page);
        data->refcount--;
        mem_data_release(data);
    }
    mem_changed(mem);
    return 0;
}

int pt_map_nothing(struct mem *mem, page_t start, pages_t pages, unsigned flags) {
    if (pages == 0) return 0;
    if ((size_t) pages > SIZE_MAX / PAGE_SIZE)
        return _ENOMEM;
    void *memory = mmap(NULL, pages * PAGE_SIZE,
            PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    return pt_map(mem, start, pages, memory, 0, flags | P_ANONYMOUS);
}

int pt_set_flags(struct mem *mem, page_t start, pages_t pages, int flags) {
    page_t end;
    if (!page_range_end(start, pages, &end))
        return _ENOMEM;
    for (page_t page = start; page < end; page++)
        if (mem_pt(mem, page) == NULL)
            return _ENOMEM;
    for (page_t page = start; page < end; page++) {
        struct pt_entry *entry = mem_pt(mem, page);
        int old_flags = entry->flags;
        entry->flags = flags;
        // check if protection is increasing
        if ((flags & ~old_flags) & (P_READ|P_WRITE)) {
            void *data = (char *) entry->data->data + entry->offset;
            // force to be page aligned
            data = (void *) ((uintptr_t) data & ~(real_page_size - 1));
            int prot = PROT_READ;
            if (flags & P_WRITE) prot |= PROT_WRITE;
            if (mprotect(data, real_page_size, prot) < 0)
                return errno_map();
        }
    }
    mem_changed(mem);
    return 0;
}

int pt_copy_on_write(struct mem *src, struct mem *dst, page_t start, page_t pages) {
    page_t end;
    if (!page_range_end(start, pages, &end))
        return _ENOMEM;

    for (page_t page = mem_next_mapped_page(src, start);
            page != BAD_PAGE && page < end;
            page = mem_next_mapped_page(src, page + 1)) {
        struct pt_entry *entry = mem_pt(src, page);
        if (pt_unmap_always(dst, page, 1) < 0)
            return -1;
        if (!(entry->flags & P_SHARED))
            entry->flags |= P_COW;
        entry->data->refcount++;
        struct pt_entry *dst_entry = mem_pt_new(dst, page);
        if (dst_entry == NULL) {
            entry->data->refcount--;
            return _ENOMEM;
        }
        dst_entry->data = entry->data;
        dst_entry->offset = entry->offset;
        dst_entry->flags = entry->flags;
    }
    mem_changed(src);
    mem_changed(dst);
    return 0;
}

static void mem_changed(struct mem *mem) {
    mem->mmu.changes++;
}

static void mem_data_release(struct data *data) {
    if (data->refcount != 0)
        return;
    // vdso wasn't allocated with mmap, it's just in our data segment
    if (data->data != vdso_data) {
        int err = munmap(data->data, data->size);
        if (err != 0)
            die("munmap(%p, %lu) failed: %s", data->data, data->size, strerror(errno));
    }
    if (data->fd != NULL)
        fd_close(data->fd);
    free(data);
}

// This version will return NULL instead of making necessary pagetable changes.
// Used by the emulator to avoid deadlocks.
static void *mem_ptr_nofault(struct mem *mem, addr_t addr, int type) {
    struct pt_entry *entry = mem_pt(mem, PAGE(addr));
    if (entry == NULL)
        return NULL;
    if (type == MEM_WRITE && !P_WRITABLE(entry->flags))
        return NULL;
    return entry->data->data + entry->offset + PGOFFSET(addr);
}

void *mem_ptr(struct mem *mem, addr_t addr, int type) {
    void *old_ptr = mem_ptr_nofault(mem, addr, type); // just for an assert

    page_t page = PAGE(addr);
    struct pt_entry *entry = mem_pt(mem, page);
    bool dropped_lock = false;

    if (entry == NULL) {
        // page does not exist
        // look to see if the next VM region is willing to grow down
        page_t p = mem_next_mapped_page(mem, page + 1);
        if (p == BAD_PAGE)
            return NULL;
        if (!(mem_pt(mem, p)->flags & P_GROWSDOWN))
            return NULL;

        // Changing memory maps must be done with the write lock. But this is
        // called with the read lock.
        // This locking stuff is copy/pasted for all the code in this function
        // which changes memory maps.
        // TODO: factor the lock/unlock code here into a new function. Do this
        // next time you touch this function.
        read_wrunlock(&mem->lock);
        write_wrlock(&mem->lock);
        pt_map_nothing(mem, page, 1, P_WRITE | P_GROWSDOWN);
        write_wrunlock(&mem->lock);
        read_wrlock(&mem->lock);
        dropped_lock = true;

        entry = mem_pt(mem, page);
    }

    if (entry != NULL && (type == MEM_WRITE || type == MEM_WRITE_PTRACE)) {
        // if page is unwritable, well tough luck
        if (type != MEM_WRITE_PTRACE && !(entry->flags & P_WRITE))
            return NULL;
        if (type == MEM_WRITE_PTRACE) {
            // TODO: Is P_WRITE really correct? The page shouldn't be writable without ptrace.
            entry->flags |= P_WRITE | P_COW;
        }
        // get rid of any compiled blocks in this page
        asbestos_invalidate_page(mem->mmu.asbestos, page);
        // if page is cow, ~~milk~~ copy it
        if (entry->flags & P_COW) {
            void *data = (char *) entry->data->data + entry->offset;
            void *copy = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);

            // copy/paste from above
            read_wrunlock(&mem->lock);
            write_wrlock(&mem->lock);
            memcpy(copy, data, PAGE_SIZE);
            pt_map(mem, page, 1, copy, 0, entry->flags &~ P_COW);
            write_wrunlock(&mem->lock);
            read_wrlock(&mem->lock);
            dropped_lock = true;
        }
    }

    void *ptr = mem_ptr_nofault(mem, addr, type);
    assert(old_ptr == NULL || old_ptr == ptr || dropped_lock || type == MEM_WRITE_PTRACE);
    return ptr;
}

static void *mem_mmu_translate(struct mmu *mmu, addr_t addr, int type) {
    return mem_ptr_nofault(container_of(mmu, struct mem, mmu), addr, type);
}

static struct mmu_ops mem_mmu_ops = {
    .translate = mem_mmu_translate,
};

int mem_segv_reason(struct mem *mem, addr_t addr) {
    struct pt_entry *pt = mem_pt(mem, PAGE(addr));
    if (pt == NULL)
        return SEGV_MAPERR_;
    return SEGV_ACCERR_;
}

size_t real_page_size;
__attribute__((constructor)) static void get_real_page_size() {
    real_page_size = sysconf(_SC_PAGESIZE);
}

void mem_coredump(struct mem *mem, const char *file) {
    int fd = open(file, O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (fd < 0) {
        perror("open");
        return;
    }

    int pages = 0;
    for (page_t page = mem_next_mapped_page(mem, 0);
            page != BAD_PAGE;
            page = mem_next_mapped_page(mem, page + 1)) {
        struct pt_entry *entry = mem_pt(mem, page);
        pages++;
        if (lseek(fd, page << PAGE_BITS, SEEK_SET) < 0) {
            perror("lseek");
            return;
        }
        if (write(fd, entry->data->data, PAGE_SIZE) < 0) {
            perror("write");
            return;
        }
    }
    printk("dumped %d pages\n", pages);
    close(fd);
}
