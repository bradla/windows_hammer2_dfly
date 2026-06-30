/*
 * hammer2_compat_windows.h - Windows compatibility shim for HAMMER2.
 *
 * This header is a Windows-specific alternative to hammer2_compat.h. It is
 * intended to isolate the porting work from the Linux compatibility shim.
 */

#ifndef _HAMMER2_COMPAT_WINDOWS_H_
#define _HAMMER2_COMPAT_WINDOWS_H_

#ifdef _WIN32

#include "hammer2_windows_port.h"
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define bzero(p, n) memset((p), 0, (n))
#define bcopy(src, dst, n) memmove((dst), (src), (n))
#define bcmp(a, b, n) memcmp((a), (b), (n))

/*
 * BSD hashinit(9): allocate a power-of-two array of list heads, initialize each
 * as an empty list, and return mask = size-1 via *hashmask.  HAMMER2 uses it
 * for the per-PFS ipdep hash; the returned array is cast to its list type
 * (one struct list_head per bucket).
 */
static inline void *
hammer2_hashinit(int elements, void *type, unsigned long *hashmask)
{
    unsigned long sz = 1;
    struct list_head *t;
    unsigned long i;

    (void)type;
    while (sz < (unsigned long)elements)
        sz <<= 1;
    t = (struct list_head *)kzalloc(sz * sizeof(struct list_head), 0);
    if (t) {
        for (i = 0; i < sz; ++i)
            INIT_LIST_HEAD(&t[i]);
    }
    if (hashmask)
        *hashmask = sz - 1;
    return t;
}

static inline void
hammer2_hashdestroy(void *table, void *type, unsigned long hashmask)
{
    (void)type;
    (void)hashmask;
    kfree(table);
}

#define hashinit(elements, type, hashmask) \
    hammer2_hashinit((elements), (type), (hashmask))
#define hashdestroy(table, type, hashmask) \
    hammer2_hashdestroy((table), (type), (hashmask))
#define vfs_hash(s, a, b) 0

/*
 * NOTE: The BSD memory-tag (M_*) sentinels, sysctl/MALLOC_DEFINE shims,
 * vfs_getopt / vfs_mountedfrom, and copyin/copyout are owned by
 * hammer2_compat.h (portable across the Linux and Windows ports).  They are
 * intentionally NOT redefined here to avoid macro redefinition conflicts.
 */

#define generate_random_uuid(uuid) do { \
    GUID __g; \
    CoCreateGuid(&__g); \
    memcpy((uuid), &__g, sizeof(*(uuid))); \
} while (0)

#else
#error "hammer2_compat_windows.h should only be included on Windows"
#endif

#endif /* _HAMMER2_COMPAT_WINDOWS_H_ */
