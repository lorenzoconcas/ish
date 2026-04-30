#include <string.h>
#include <sys/stat.h>
#include <limits.h>

#include "kernel/calls.h"
#include "kernel/errno.h"
#include "kernel/fs.h"
#include "fs/fd.h"
#include "fs/path.h"

struct newstat64 stat_convert_newstat64(struct statbuf stat) {
    struct newstat64 newstat;
    newstat.dev = stat.dev;
    newstat.fucked_ino = stat.inode;
    newstat.ino = stat.inode;
    newstat.mode = stat.mode;
    newstat.nlink = stat.nlink;
    newstat.uid = stat.uid;
    newstat.gid = stat.gid;
    newstat.rdev = stat.rdev;
    newstat.size = stat.size;
    newstat.blksize = stat.blksize;
    newstat.blocks = stat.blocks;
    newstat.atime = stat.atime;
    newstat.atime_nsec = stat.atime_nsec;
    newstat.mtime = stat.mtime;
    newstat.mtime_nsec = stat.mtime_nsec;
    newstat.ctime = stat.ctime;
    newstat.ctime_nsec = stat.ctime_nsec;
    return newstat;
}

static struct stat_x86_64 stat_convert_x86_64(struct statbuf stat) {
    struct stat_x86_64 x64stat = {};
    x64stat.dev = stat.dev;
    x64stat.ino = stat.inode;
    x64stat.nlink = stat.nlink;
    x64stat.mode = stat.mode;
    x64stat.uid = stat.uid;
    x64stat.gid = stat.gid;
    x64stat.rdev = stat.rdev;
    x64stat.size = stat.size;
    x64stat.blksize = stat.blksize;
    x64stat.blocks = stat.blocks;
    x64stat.atime = stat.atime;
    x64stat.atime_nsec = stat.atime_nsec;
    x64stat.mtime = stat.mtime;
    x64stat.mtime_nsec = stat.mtime_nsec;
    x64stat.ctime = stat.ctime;
    x64stat.ctime_nsec = stat.ctime_nsec;
    return x64stat;
}

int generic_statat(struct fd *at, const char *path_raw, struct statbuf *stat, bool follow_links) {
    char path[MAX_PATH];
    int err = path_normalize(at, path_raw, path, follow_links ? N_SYMLINK_FOLLOW : N_SYMLINK_NOFOLLOW);
    if (err < 0)
        return err;
    struct mount *mount = find_mount_and_trim_path(path);
    memset(stat, 0, sizeof(*stat));
    err = mount->fs->stat(mount, path, stat);
    mount_release(mount);
    return err;
}

// TODO get rid of this and maybe everything else in the file
static struct fd *at_fd(fd_t f) {
    if (f == AT_FDCWD_)
        return AT_PWD;
    return f_get(f);
}

static dword_t sys_stat_path(fd_t at_f, addr_t path_addr, addr_t statbuf_addr, bool follow_links) {
    int err;
    char path[MAX_PATH];
    if (user_read_string(path_addr, path, sizeof(path)))
        return _EFAULT;
    STRACE("stat(at=%d, path=\"%s\", statbuf=0x%x, follow_links=%d)", at_f, path, statbuf_addr, follow_links);
    struct fd *at = at_fd(at_f);
    if (at == NULL)
        return _EBADF;
    struct statbuf stat = {};
    if ((err = generic_statat(at, path, &stat, follow_links)) < 0)
        return err;
    struct newstat64 newstat = stat_convert_newstat64(stat);
    if (user_put(statbuf_addr, newstat))
        return _EFAULT;
    return 0;
}

dword_t sys_stat64(addr_t path_addr, addr_t statbuf_addr) {
    return sys_stat_path(AT_FDCWD_, path_addr, statbuf_addr, true);
}

dword_t sys_lstat64(addr_t path_addr, addr_t statbuf_addr) {
    return sys_stat_path(AT_FDCWD_, path_addr, statbuf_addr, false);
}

dword_t sys_fstatat64(fd_t at, addr_t path_addr, addr_t statbuf_addr, dword_t flags) {
    return sys_stat_path(at, path_addr, statbuf_addr, !(flags & AT_SYMLINK_NOFOLLOW_));
}

dword_t sys_fstat64(fd_t fd_no, addr_t statbuf_addr) {
    STRACE("fstat64(%d, 0x%x)", fd_no, statbuf_addr);
    struct fd *fd = f_get(fd_no);
    if (fd == NULL)
        return _EBADF;
    struct statbuf stat = {};
    int err = fd->mount->fs->fstat(fd, &stat);
    if (err < 0)
        return err;
    struct newstat64 newstat = stat_convert_newstat64(stat);
    if (user_put(statbuf_addr, newstat))
        return _EFAULT;
    return 0;
}

static dword_t sys_stat_path_x86_64(fd_t at_f, addr_t path_addr, addr_t statbuf_addr, bool follow_links) {
    int err;
    char path[MAX_PATH];
    if (user_read_string(path_addr, path, sizeof(path)))
        return _EFAULT;
    STRACE("stat_x86_64(at=%d, path=\"%s\", statbuf=0x%llx, follow_links=%d)",
            at_f, path, (unsigned long long) statbuf_addr, follow_links);
    struct fd *at = at_fd(at_f);
    if (at == NULL)
        return _EBADF;
    struct statbuf stat = {};
    if ((err = generic_statat(at, path, &stat, follow_links)) < 0)
        return err;
    struct stat_x86_64 x64stat = stat_convert_x86_64(stat);
    if (user_put(statbuf_addr, x64stat))
        return _EFAULT;
    return 0;
}

dword_t sys_stat_x86_64(addr_t path_addr, addr_t statbuf_addr) {
    return sys_stat_path_x86_64(AT_FDCWD_, path_addr, statbuf_addr, true);
}

dword_t sys_lstat_x86_64(addr_t path_addr, addr_t statbuf_addr) {
    return sys_stat_path_x86_64(AT_FDCWD_, path_addr, statbuf_addr, false);
}

dword_t sys_fstat_x86_64(fd_t fd_no, addr_t statbuf_addr) {
    STRACE("fstat_x86_64(%d, 0x%llx)", fd_no, (unsigned long long) statbuf_addr);
    struct fd *fd = f_get(fd_no);
    if (fd == NULL)
        return _EBADF;
    struct statbuf stat = {};
    int err = fd->mount->fs->fstat(fd, &stat);
    if (err < 0)
        return err;
    struct stat_x86_64 x64stat = stat_convert_x86_64(stat);
    if (user_put(statbuf_addr, x64stat))
        return _EFAULT;
    return 0;
}

dword_t sys_newfstatat_x86_64(fd_t at, addr_t path_addr, addr_t statbuf_addr, dword_t flags) {
    return sys_stat_path_x86_64(at, path_addr, statbuf_addr, !(flags & AT_SYMLINK_NOFOLLOW_));
}

dword_t sys_statx(fd_t at_f, addr_t path_addr, int_t flags, uint_t mask, addr_t statx_addr) {
    char path[MAX_PATH];
    if (user_read_string(path_addr, path, sizeof(path)))
        return _EFAULT;
    struct fd *at = at_fd(at_f);
    if (at == NULL)
        return _EBADF;

    STRACE("statx(at=%d, path=\"%s\", flags=%d, mask=%d, statx=0x%x)", at_f, path, flags, mask, statx_addr);

    struct statbuf stat = {};

    if ((flags & AT_EMPTY_PATH_) && strcmp(path, "") == 0) {
        struct fd *fd = at;
        int err = fd->mount->fs->fstat(fd, &stat);
        if (err < 0)
            return err;
    } else {
        bool follow_links = !(flags & AT_SYMLINK_NOFOLLOW_);
        int err = generic_statat(at, path, &stat, follow_links);
        if (err < 0)
            return err;
    }

    // for now, ignore the requested mask and just fill in the same fields as stat returns
    struct statx_ statx = {};
    statx.mask = STATX_BASIC_STATS_;
    statx.blksize = stat.blksize;
    statx.nlink = stat.nlink;
    statx.uid = stat.uid;
    statx.gid = stat.gid;
    statx.mode = stat.mode;
    statx.ino = stat.inode;
    statx.size = stat.size;
    statx.blocks = stat.blocks;
    statx.atime.sec = stat.atime;
    statx.atime.nsec = stat.atime_nsec;
    statx.mtime.sec = stat.mtime;
    statx.mtime.nsec = stat.mtime_nsec;
    statx.ctime.sec = stat.ctime;
    statx.ctime.nsec = stat.ctime_nsec;
    statx.rdev_major = dev_major(stat.rdev);
    statx.rdev_minor = dev_minor(stat.rdev);
    statx.dev_major = dev_major(stat.dev);
    statx.dev_minor = dev_minor(stat.dev);

    if (user_put(statx_addr, statx))
        return _EFAULT;
    return 0;
}
