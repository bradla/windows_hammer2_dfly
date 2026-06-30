/*
 * hammer2_os_windows.h - Windows OS compatibility shim for HAMMER2.
 *
 * This header provides basic Windows lock primitives for the port.
 */

#ifndef _HAMMER2_OS_WINDOWS_H_
#define _HAMMER2_OS_WINDOWS_H_

#ifdef _WIN32

#include "hammer2_windows_port.h"
#include <windows.h>
#include <synchapi.h>
#include <processthreadsapi.h>
#include <stdint.h>
#include <stdbool.h>

typedef CRITICAL_SECTION hammer2_mtx_t;
typedef CRITICAL_SECTION hammer2_lk_t;
typedef CONDITION_VARIABLE hammer2_lkc_t;
typedef CRITICAL_SECTION hammer2_spin_t;

#define current GetCurrentThreadId()

static inline void
hammer2_mtx_init(hammer2_mtx_t *p, const char *s)
{
    (void)s;
    InitializeCriticalSection(p);
}

static inline void
hammer2_mtx_init_recurse(hammer2_mtx_t *p, const char *s)
{
    hammer2_mtx_init(p, s);
}

static inline void
hammer2_mtx_ex(hammer2_mtx_t *p)
{
    EnterCriticalSection(p);
}

static inline void
hammer2_mtx_sh(hammer2_mtx_t *p)
{
    EnterCriticalSection(p);
}

static inline void
hammer2_mtx_unlock(hammer2_mtx_t *p)
{
    LeaveCriticalSection(p);
}

static inline int
hammer2_mtx_refs(hammer2_mtx_t *p)
{
    /* CRITICAL_SECTION.RecursionCount is the owner's hold depth (0 = free). */
    return (int)p->RecursionCount;
}

static inline void
hammer2_mtx_destroy(hammer2_mtx_t *p)
{
    DeleteCriticalSection(p);
}

static inline int
hammer2_mtx_sleep(void *c, hammer2_mtx_t *p, const char *s, int timo)
{
    (void)c;
    (void)s;

    LeaveCriticalSection(p);
    Sleep(timo ? timo : 1);
    EnterCriticalSection(p);
    return 0;
}

static inline void
hammer2_mtx_wakeup(void *c)
{
    (void)c;
}

static inline int
hammer2_spin_ssleep(const volatile void *ident, hammer2_spin_t *spin,
    int flags, const char *wmesg, int timo)
{
    (void)ident;
    (void)flags;
    (void)wmesg;

    LeaveCriticalSection(spin);
    Sleep(timo ? timo : 1);
    EnterCriticalSection(spin);
    return 0;
}

static inline int
hammer2_mtx_owned(hammer2_mtx_t *p)
{
    /* True iff the current thread holds the lock. */
    return p->OwningThread == (HANDLE)(ULONG_PTR)GetCurrentThreadId() &&
           p->RecursionCount > 0;
}

static inline int
hammer2_mtx_ex_try(hammer2_mtx_t *p)
{
    return TryEnterCriticalSection(p) ? 0 : 1;
}

static inline int
hammer2_mtx_sh_try(hammer2_mtx_t *p)
{
    return hammer2_mtx_ex_try(p);
}

static inline int
hammer2_mtx_upgrade_try(hammer2_mtx_t *p)
{
    (void)p;
    return 0;
}

/*
 * Temporarily fully release an exclusive hold, returning the prior depth so it
 * can be restored.  CRITICAL_SECTIONs are recursive, so leave once per level.
 */
static inline int
hammer2_mtx_temp_release(hammer2_mtx_t *p)
{
    LeaveCriticalSection(p);
    return 1;
}

static inline void
hammer2_mtx_temp_restore(hammer2_mtx_t *p, int x)
{
    (void)x;
    EnterCriticalSection(p);
}

/*
 * Convenience aliases mirroring the Linux shim in hammer2_os.h.
 */
#define hammer2_mtx_lock(p)     hammer2_mtx_ex(p)
#define hammer2_mtx_shunlock(p) hammer2_mtx_unlock(p)

/*
 * Spinlock primitives.  Modeled on CRITICAL_SECTIONs; Windows doesn't
 * distinguish shared/exclusive on them.
 */
#define hammer2_spin_ex(s)      EnterCriticalSection(s)
#define hammer2_spin_unex(s)    LeaveCriticalSection(s)
#define hammer2_spin_sh(s)      EnterCriticalSection(s)
#define hammer2_spin_unsh(s)    LeaveCriticalSection(s)

static inline void
hammer2_spin_init(hammer2_spin_t *s, const char *msg)
{
    (void)msg;
    InitializeCriticalSection(s);
}

static inline void
hammer2_spin_destroy(hammer2_spin_t *s)
{
    DeleteCriticalSection(s);
}

/*
 * rw_semaphore-style "lk" primitives.  hammer2_lk_t is a CRITICAL_SECTION.
 */
#define hammer2_lk_ex(l)        EnterCriticalSection(l)
#define hammer2_lk_unlock(l)    LeaveCriticalSection(l)
#define hammer2_lk_assert_ex(l) ((void)(l))

/*
 * Additional lockmgr-style names used directly by hammer2.h call sites.
 * hammer2_lk_t is a CRITICAL_SECTION (recursive, exclusive-only here).
 */
#define hammer2_lk_init(l, ...)	InitializeCriticalSection(l)
#define hammer2_lk_destroy(l)	DeleteCriticalSection(l)
#define hammer2_lk_lock(l, flags) ((void)(flags), EnterCriticalSection(l))
#define hammer2_lk_sh(l)	EnterCriticalSection(l)
#define hammer2_lk_shunlock(l)	LeaveCriticalSection(l)

#define hammer2_spin_lock(s)	EnterCriticalSection(s)
#define hammer2_spin_unlock(s)	LeaveCriticalSection(s)

#define hammer2_mtx_assert_unlocked(p)  ((void)(p))

/*
 * Condition-variable style sleep/wakeup.  hammer2_lkc_t is a
 * CONDITION_VARIABLE associated with the hammer2_lk_t (CRITICAL_SECTION).
 */
static inline void
hammer2_lkc_init(hammer2_lkc_t *c, const char *s)
{
    (void)s;
    InitializeConditionVariable(c);
}

static inline void
hammer2_lkc_destroy(hammer2_lkc_t *c)
{
    (void)c;
}

static inline int
hammer2_lkc_sleep(hammer2_lkc_t *c, hammer2_lk_t *lk, const char *s, int timo)
{
    DWORD ms;

    (void)s;
    ms = timo ? (DWORD)timo : INFINITE;
    if (!SleepConditionVariableCS(c, lk, ms))
        return -1; /* timeout or error; callers re-check their condition */
    return 0;
}

static inline void
hammer2_lkc_wakeup(hammer2_lkc_t *c)
{
    WakeAllConditionVariable(c);
}

#else
#error "hammer2_os_windows.h should only be included on Windows"
#endif

#endif /* _HAMMER2_OS_WINDOWS_H_ */
