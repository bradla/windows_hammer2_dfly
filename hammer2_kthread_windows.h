/*
 * hammer2_kthread_windows.h - Windows kthread / waitqueue / misc kernel shims.
 *
 * HAMMER2's worker model (hammer2_thread.c) and the DMSG layer (kern_dmsg.c)
 * use the Linux kthread + waitqueue API.  This header maps that surface onto
 * Win32 threads and condition variables so the XOP worker threads actually run.
 */

#ifndef _HAMMER2_KTHREAD_WINDOWS_H_
#define _HAMMER2_KTHREAD_WINDOWS_H_

#include "hammer2_windows_port.h"

/* Linux kthread handle.  hammer2_thread.c stores it and checks IS_ERR(). */
struct task_struct {
	HANDLE	kt_handle;
	int	(*kt_fn)(void *);
	void	*kt_arg;
};

/* struct socket is only referenced by the (inactive) DMSG path. */
struct socket;

/*
 * Waitqueue.  wait_queue_head_t (hammer2_windows_port.h) wraps a Win32
 * CONDITION_VARIABLE + CRITICAL_SECTION.  h2_wq_wait() blocks for up to a
 * bounded poll interval so a missed wake can never hang the caller; every
 * wait_event*() re-checks its condition in a loop.
 */
void h2_wq_wait(wait_queue_head_t *wq, int timeout_ms);

static inline void
init_waitqueue_head(wait_queue_head_t *wq)
{
	InitializeConditionVariable(&wq->cv);
	InitializeCriticalSection(&wq->lk);
	wq->inited = 1;
}

static inline void
wake_up(wait_queue_head_t *wq)
{
	if (!wq->inited)
		return;
	EnterCriticalSection(&wq->lk);
	WakeAllConditionVariable(&wq->cv);
	LeaveCriticalSection(&wq->lk);
}
#define wake_up_all(wq)			wake_up(wq)
#define wake_up_interruptible(wq)	wake_up(wq)

#define wait_event(wq, cond) \
	do { while (!(cond)) h2_wq_wait(&(wq), 0); } while (0)

/* Returns >0 if the condition became true, 0 on timeout (callers re-check). */
#define wait_event_timeout(wq, cond, to) \
	((cond) ? 1 : (h2_wq_wait(&(wq), (int)(to)), (cond) ? 1 : 0))

#define wait_event_interruptible(wq, cond) \
	(wait_event((wq), (cond)), 0)
#define wait_event_interruptible_timeout(wq, cond, to) \
	wait_event_timeout((wq), (cond), (to))

/* Thread creation.  threadfn return value is discarded (BSD STOP-flag model). */
struct task_struct *kthread_run(int (*threadfn)(void *), void *data,
				const char *namefmt, ...);
int  kthread_stop(struct task_struct *k);
int  kthread_should_stop(void);

long schedule_timeout_uninterruptible(long timo);

void panic(const char *fmt, ...);

/* DMSG file/socket I/O -- stubs; the cluster network path is inactive for a
 * local mount.  Provided so the core links. */
ssize_t kernel_read(struct file *file, void *buf, size_t count, loff_t *pos);
ssize_t kernel_write(struct file *file, const void *buf, size_t count,
		     loff_t *pos);
struct socket *sock_from_file(struct file *file);
int kernel_sock_shutdown(struct socket *sock, int how);

#endif /* _HAMMER2_KTHREAD_WINDOWS_H_ */
