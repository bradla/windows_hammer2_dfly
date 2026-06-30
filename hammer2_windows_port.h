/*
 * hammer2_windows_port.h - Windows compatibility stubs for the Windows port.
 *
 * This header is intentionally minimal. It provides a lightweight foundation
 * for porting the Linux-based HAMMER2 source to Windows. Most definitions are
 * placeholders and will need replacement by Windows kernel or user-mode
 * filesystem equivalents during subsequent porting work.
 */

#ifndef _HAMMER2_WINDOWS_PORT_H_
#define _HAMMER2_WINDOWS_PORT_H_

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _CRT_RAND_S
#define _CRT_RAND_S	/* enable rand_s() in <stdlib.h> */
#endif
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>

#ifndef HZ
#define HZ 1000
#endif

/*
 * GCC/Clang attribute and kernel annotation shims for MSVC.
 *
 * NOTE: the on-disk HAMMER2 structures are documented as naturally aligned, so
 * mapping __packed to nothing preserves their layout on MSVC.  If a future
 * struct relies on true byte-packing, wrap it in #pragma pack(push,1)/pop.
 */
#ifndef __packed
#define __packed
#endif
#ifndef __aligned
#define __aligned(x)
#endif
#ifndef __attribute__
#define __attribute__(x)
#endif
#ifndef __unused
#define __unused
#endif
#ifndef __maybe_unused
#define __maybe_unused
#endif
#ifndef __user
#define __user
#endif
#ifndef __force
#define __force
#endif
#ifndef __iomem
#define __iomem
#endif
#ifndef __always_inline
#define __always_inline __forceinline
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#ifndef NAME_MAX
#define NAME_MAX 255
#endif

/*
 * Linux-style ioctl command encoding (<linux/ioctl.h>).  HAMMER2 only uses
 * these to derive stable command numbers; the size/dir bits are preserved so
 * the values match the Linux build.
 */
#ifndef _IOC
#define _IOC_NRBITS	8
#define _IOC_TYPEBITS	8
#define _IOC_SIZEBITS	14
#define _IOC_DIRBITS	2
#define _IOC_NRSHIFT	0
#define _IOC_TYPESHIFT	(_IOC_NRSHIFT + _IOC_NRBITS)
#define _IOC_SIZESHIFT	(_IOC_TYPESHIFT + _IOC_TYPEBITS)
#define _IOC_DIRSHIFT	(_IOC_SIZESHIFT + _IOC_SIZEBITS)
#define _IOC_NONE	0U
#define _IOC_WRITE	1U
#define _IOC_READ	2U
#define _IOC(dir, type, nr, size) \
	(((dir) << _IOC_DIRSHIFT) | ((type) << _IOC_TYPESHIFT) | \
	 ((nr) << _IOC_NRSHIFT) | ((size) << _IOC_SIZESHIFT))
#define _IO(type, nr)		_IOC(_IOC_NONE, (type), (nr), 0)
#define _IOR(type, nr, sz)	_IOC(_IOC_READ, (type), (nr), sizeof(sz))
#define _IOW(type, nr, sz)	_IOC(_IOC_WRITE, (type), (nr), sizeof(sz))
#define _IOWR(type, nr, sz)	_IOC(_IOC_READ | _IOC_WRITE, (type), (nr), sizeof(sz))
#endif

typedef unsigned int u_int;
typedef unsigned long u_long;
typedef unsigned char u_char;
typedef unsigned short u_short;

/* Linux kernel fixed-width integer aliases used by the on-disk format. */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef uint8_t  __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef uint64_t __u64;
typedef int8_t   __s8;
typedef int16_t  __s16;
typedef int32_t  __s32;
typedef int64_t  __s64;
/* Endian-annotated aliases are plain integers in this host-endian port. */
typedef uint16_t __le16;
typedef uint32_t __le32;
typedef uint64_t __le64;
typedef uint16_t __be16;
typedef uint32_t __be32;
typedef uint64_t __be64;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef uint8_t  uint8;
/* BSD u_intN_t spellings. */
typedef uint8_t  u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
typedef uint64_t u_int64_t;
/* POSIX ssize_t (MSVC only provides SSIZE_T). */
#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef long long ssize_t;
#endif
typedef unsigned long dev_t;
typedef unsigned long ino_t;
typedef unsigned long nlink_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef long pid_t;
typedef unsigned long mode_t;
typedef unsigned long long sector_t;
typedef long long daddr_t;	/* BSD disk block address */

/* Linux time-of-day struct (shared guard with hammer2_compat.h). */
#ifndef _HAMMER2_HAS_TIMESPEC64
#define _HAMMER2_HAS_TIMESPEC64
struct timespec64 {
    int64_t     tv_sec;
    long        tv_nsec;
};
#endif
#ifndef _OFF_T_DEFINED
#define _OFF_T_DEFINED
typedef long long off_t;
#endif

#ifndef ECONNRESET
#define ECONNRESET WSAECONNRESET
#endif

#ifndef EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
#endif

#ifndef ENOMEM
#define ENOMEM ERROR_NOT_ENOUGH_MEMORY
#endif

typedef int gfp_t;
#define GFP_KERNEL 0
#define GFP_ATOMIC 1
#define M_WAITOK GFP_KERNEL
#define M_NOWAIT GFP_ATOMIC
#define M_ZERO 0x80000000U
#define M_NULL NULL
#define M_HAMMER NULL
#define M_HAMMER2 NULL
#define M_TEMP NULL
#define M_FORCE 0
#define M_VOLHDRS NULL
#define M_IGNINO 0
#define M_PERMANENT 0
#define M_END 0
#define M_LEVEL 0
#define M_X 0

static inline void *kmalloc(size_t size, int flags)
{
    (void)flags;
    return malloc(size);
}

static inline void *kvmalloc(size_t size, int flags)
{
    (void)flags;
    return malloc(size);
}

static inline void *kvzalloc(size_t size, int flags)
{
    (void)flags;
    return calloc(1, size);
}

static inline void kvfree(void *ptr)
{
    free(ptr);
}

static inline void kfree(void *ptr)
{
    free(ptr);
}

static inline void *krealloc(void *ptr, size_t size, int flags)
{
    (void)flags;
    return realloc(ptr, size);
}

#define hmalloc(sz, tag, fl) hammer2_kmalloc((sz), (tag), (fl))
#define hfree(p, ...) kvfree((p))
#define hrealloc(p, sz, tag, fl) hammer2_krealloc((p), (sz), (tag), (fl))

static inline void *hammer2_kmalloc(size_t sz, void *tag, unsigned int flags)
{
    (void)tag;
    if (flags & M_ZERO)
        return calloc(1, sz);
    return malloc(sz);
}

static inline void *hammer2_krealloc(void *p, size_t sz, void *tag, unsigned int flags)
{
    (void)tag;
    (void)flags;
    return realloc(p, sz);
}

/*
 * NOTE: The BSD memory-tag / sysctl / vfs_* / copyin-copyout shims are owned
 * by hammer2_compat.h so that the Linux and Windows ports share one portable
 * definition.  They are intentionally NOT redefined here to avoid macro
 * redefinition conflicts.
 */

struct list_head {
    struct list_head *next;
    struct list_head *prev;
};
typedef struct list_head list_head;

#define container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))
#define list_entry(ptr, type, member) container_of(ptr, type, member)
#define list_first_entry(head, type, member) list_entry((head)->next, type, member)
#define list_last_entry(head, type, member) list_entry((head)->prev, type, member)
#define list_first_entry_or_null(head, type, member) \
    ((head)->next == (head) ? NULL : list_entry((head)->next, type, member))
#define list_empty(head) ((head)->next == (head))
#define list_is_last(entry, head) ((entry)->next == (head))

#if defined(__GNUC__) || defined(__clang__)
#define list_next_entry(entry, member) list_entry((entry)->member.next, __typeof__(*entry), member)
#else
/* MSVC: typeof is available under /std:clatest (C23). */
#define list_next_entry(entry, member) list_entry((entry)->member.next, typeof(*entry), member)
#endif

#define INIT_LIST_HEAD(ptr) do { (ptr)->next = (ptr); (ptr)->prev = (ptr); } while (0)
#define list_add(entry, head) do { \
    (entry)->next = (head)->next; \
    (entry)->prev = (head); \
    (head)->next->prev = (entry); \
    (head)->next = (entry); \
} while (0)
#define list_add_tail(entry, head) do { \
    (entry)->next = (head); \
    (entry)->prev = (head)->prev; \
    (head)->prev->next = (entry); \
    (head)->prev = (entry); \
} while (0)
#define list_del(entry) do { \
    (entry)->next->prev = (entry)->prev; \
    (entry)->prev->next = (entry)->next; \
} while (0)

/* Move all of `list` onto the tail of `head`, then re-init `list` as empty. */
static inline void list_splice_tail_init(struct list_head *list,
                                         struct list_head *head)
{
    if (list->next != list) {
        struct list_head *first = list->next;
        struct list_head *last = list->prev;
        struct list_head *at = head->prev;
        first->prev = at;
        at->next = first;
        last->next = head;
        head->prev = last;
        list->next = list;
        list->prev = list;
    }
}

typedef struct {
    volatile long counter;
} atomic_t;

static inline void atomic_set(atomic_t *v, int i)
{
    v->counter = i;
}

static inline int atomic_read(const atomic_t *v)
{
    return (int)v->counter;
}

static inline int atomic_inc_return(atomic_t *v)
{
    return InterlockedIncrement(&v->counter);
}

static inline int atomic_dec_and_test(atomic_t *v)
{
    return InterlockedDecrement(&v->counter) == 0;
}

static inline void atomic_add(int i, atomic_t *v)
{
    InterlockedAdd(&v->counter, (LONG)i);
}

static inline void atomic_sub(int i, atomic_t *v)
{
    InterlockedAdd(&v->counter, -(LONG)i);
}

static inline void atomic_inc(atomic_t *v)
{
    InterlockedIncrement(&v->counter);
}

static inline void atomic_dec(atomic_t *v)
{
    InterlockedDecrement(&v->counter);
}

static inline int atomic_add_return(int i, atomic_t *v)
{
    return (int)InterlockedAdd(&v->counter, (LONG)i);
}

static inline int atomic_sub_and_test(int i, atomic_t *v)
{
    return (int)InterlockedAdd(&v->counter, -(LONG)i) == 0;
}

#define READ_ONCE(x) (x)
#define WRITE_ONCE(x, val) ((x) = (val))

#define KERN_EMERG ""
#define KERN_ALERT ""
#define KERN_CRIT ""
#define KERN_ERR ""
#define KERN_WARNING ""
#define KERN_NOTICE ""
#define KERN_INFO ""
#define KERN_DEBUG ""
#define KERN_CONT ""

#define printk printf
#define pr_err printf
#define pr_warn printf
#define pr_info printf
#define pr_debug printf
#define printk_ratelimited printf

/*
 * Linux kernel branch-prediction / debug primitives mapped to MSVC.
 */
#ifndef likely
#define likely(x)	(x)
#define unlikely(x)	(x)
#endif

#define barrier()	_ReadWriteBarrier()
#ifndef cpu_relax
#define cpu_relax()	YieldProcessor()
#endif

/* GCC builtin used by hammer2_lz4.c. */
#ifndef __builtin_expect
#define __builtin_expect(expr, expected)	(expr)
#endif

/* Power-of-two round down/up (Linux <linux/kernel.h>). */
#ifndef round_down
#define round_down(x, y)	((x) & ~((__typeof__(x))(y) - 1))
#endif
#ifndef round_up
#define round_up(x, y)		(((x) + ((__typeof__(x))(y) - 1)) & ~((__typeof__(x))(y) - 1))
#endif

/* Bounded string copy (Linux strscpy: returns copied length or -E2BIG). */
static inline ssize_t strscpy(char *dst, const char *src, size_t size)
{
    size_t i = 0;
    if (size == 0)
        return -7 /* -E2BIG */;
    for (; i < size - 1 && src[i]; ++i)
        dst[i] = src[i];
    dst[i] = '\0';
    return src[i] ? -7 : (ssize_t)i;
}

static inline char *kstrdup(const char *s, int flags)
{
    (void)flags;
    return s ? _strdup(s) : NULL;
}
static inline char *hstrdup(const char *s) { return kstrdup(s, 0); }
static inline void  hstrfree(char *s)      { free(s); }

/* POSIX case-insensitive compares -> MSVC spellings. */
#ifndef strcasecmp
#define strcasecmp  _stricmp
#endif
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif

/* Linux jiffies / tick source, in milliseconds (HZ == 1000). */
#define jiffies		((unsigned long)GetTickCount64())

#define unreachable()	__assume(0)

/*
 * BUG() must be self-contained: hammer2_panic() (hammer2_compat.h) calls BUG()
 * at the end, so routing BUG() back through hpanic()/hammer2_panic() would form
 * a macro cycle.  Abort directly instead.
 */
static inline __declspec(noreturn) void hammer2_win_bug(const char *file, int line)
{
    fprintf(stderr, "HAMMER2 BUG at %s:%d\n", file, line);
    abort();
}
#define BUG()		hammer2_win_bug(__FILE__, __LINE__)
#define BUG_ON(c)	do { if (c) BUG(); } while (0)
#define WARN_ON(c)	((c) ? (printf("HAMMER2 WARN_ON(%s) at %s:%d\n", #c, __FILE__, __LINE__), 1) : 0)
#define WARN_ON_ONCE(c)	WARN_ON(c)

/* Zeroing allocator companion to kmalloc(). */
static inline void *kzalloc(size_t size, int flags)
{
    (void)flags;
    return calloc(1, size);
}

/*
 * Time: Linux's ktime_get_real_ts64() returns wall-clock time.  Map onto the
 * Win32 system clock (100ns ticks since 1601), converted to the Unix epoch.
 */
static inline void ktime_get_real_ts64(struct timespec64 *ts)
{
    FILETIME ft;
    ULARGE_INTEGER u;
    uint64_t t100ns;

    GetSystemTimePreciseAsFileTime(&ft);
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    /* 116444736000000000 = 100ns ticks between 1601-01-01 and 1970-01-01. */
    t100ns = u.QuadPart - 116444736000000000ULL;
    ts->tv_sec = (int64_t)(t100ns / 10000000ULL);
    ts->tv_nsec = (long)((t100ns % 10000000ULL) * 100);
}

/* Signals: no per-thread signal queue in this port; never interrupted. */
#ifndef SIGINT
#define SIGINT 2
#endif
#define signal_pending(t)	((void)(t), 0)

/* Fill a buffer with random bytes (best-effort, userspace CRT). */
static inline void get_random_bytes(void *buf, size_t n)
{
    unsigned char *p = (unsigned char *)buf;
    size_t i;
    for (i = 0; i < n; ++i) {
        unsigned int r = 0;
        if (rand_s(&r) != 0)
            r = (unsigned int)rand();
        p[i] = (unsigned char)(r & 0xff);
    }
}

/* Page geometry (4 KiB) used for I/O sizing math. */
#ifndef PAGE_SHIFT
#define PAGE_SHIFT	12
#endif
#ifndef PAGE_SIZE
#define PAGE_SIZE	(1UL << PAGE_SHIFT)
#endif
#ifndef PAGE_MASK
#define PAGE_MASK	(PAGE_SIZE - 1)
#endif

/* BSD/POSIX scatter-gather vector. */
#ifndef _HAMMER2_HAS_IOVEC
#define _HAMMER2_HAS_IOVEC
struct iovec {
    void   *iov_base;
    size_t  iov_len;
};
#endif

/*
 * Linux module registration macros -- no-ops in this port.  A real Windows
 * driver registers via DriverEntry instead.
 */
#define MODULE_LICENSE(s)	struct __h2_modlic { int unused; }
#define MODULE_AUTHOR(s)	struct __h2_modauth { int unused; }
#define MODULE_DESCRIPTION(s)	struct __h2_moddesc { int unused; }
#define MODULE_INFO(tag, s)	struct __h2_modinfo_##tag { int unused; }
#define MODULE_ALIAS(s)		struct __h2_modalias { int unused; }
#define MODULE_ALIAS_FS(s)	struct __h2_modaliasfs { int unused; }
#define module_init(fn)		int __h2_module_init_unused = 0
#define module_exit(fn)		int __h2_module_exit_unused = 0
#define module_param(name, type, perm)		struct __h2_modparam_##name { int unused; }
#define module_param_named(a, b, type, perm)	struct __h2_modparamn_##a { int unused; }
#define MODULE_PARM_DESC(name, desc)		struct __h2_modparmd_##name { int unused; }
#define __init
#define __exit

/*
 * copyin/copyout, vfs_getopt and vfs_mountedfrom are owned by
 * hammer2_compat.h (portable across the Linux and Windows ports).
 */

static inline void hpanic(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    abort();
}

/*
 * Linux scalar type aliases used by the VFS-facing portions of HAMMER2.
 */
typedef long long loff_t;
typedef unsigned short umode_t;
typedef uint32_t kuid_t;
typedef uint32_t kgid_t;

/* Linux uuid_t is a 16-byte opaque blob (distinct from BSD `struct uuid`). */
#ifndef _HAMMER2_HAS_UUID_T
#define _HAMMER2_HAS_UUID_T
typedef struct { uint8_t b[16]; } uuid_t;
#endif

/*
 * Linux wait_queue_head_t shim.  Backed by a Win32 condition variable + lock
 * so the wait_event()/wake_up() helpers (added by the OS shim) can block and
 * signal.  Header-level code only needs the type to size struct fields.
 */
typedef struct hammer2_waitqueue {
    CONDITION_VARIABLE  cv;
    CRITICAL_SECTION    lk;
    int                 inited;
} wait_queue_head_t;

/*
 * Minimal Linux VFS object stubs.  Only the fields HAMMER2 actually touches
 * are provided; a real Windows FS driver replaces these with FILE_OBJECT /
 * FCB / VCB plumbing.  These are forward-declared where only a pointer is
 * needed and defined where a member is dereferenced.
 */
struct super_block;
struct file;
struct dentry;
struct block_device;
struct task_struct;
struct address_space;

/* Linux block buffer; the port touches only b_data / b_offset. */
struct buffer_head {
    char    *b_data;
    loff_t   b_offset;
    size_t   b_size;
    sector_t b_blocknr;
    struct block_device *b_bdev;
};

/* Common fcntl open flags (MSVC spells them _O_*). */
#ifndef O_APPEND
#define O_APPEND	0x0008
#endif
#ifndef O_TRUNC
#define O_TRUNC		0x0200
#endif
#ifndef O_SYNC
#define O_SYNC		0x0800
#endif

/* lseek whence extensions (Linux values). */
#ifndef SEEK_DATA
#define SEEK_DATA	3
#endif
#ifndef SEEK_HOLE
#define SEEK_HOLE	4
#endif
#ifndef PIPE_BUF
#define PIPE_BUF	4096
#endif

struct inode {
    void                *i_private;     /* HAMMER2 stashes hammer2_inode_t * */
    struct super_block  *i_sb;
    unsigned long        i_ino;
    umode_t              i_mode;
    unsigned int         i_nlink;
    kuid_t               i_uid;
    kgid_t               i_gid;
    loff_t               i_size;
    dev_t                i_rdev;
    void                *i_mapping;
    atomic_t             i_count;
    struct timespec64    i_atime;
    struct timespec64    i_mtime;
    struct timespec64    i_ctime;
};

/*
 * Linux inode refcount / writeback helpers.  This port does not yet maintain a
 * Windows inode cache, so igrab/iput are identity/no-ops and write_inode_now
 * (used only by the sync path) succeeds trivially.
 */
static inline struct inode *igrab(struct inode *inode) { return inode; }
static inline void iput(struct inode *inode) { (void)inode; }
static inline int write_inode_now(struct inode *inode, int sync)
{
    (void)inode; (void)sync;
    return 0;
}

#endif /* _WIN32 */

#endif /* _HAMMER2_WINDOWS_PORT_H_ */
