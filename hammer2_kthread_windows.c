/*
 * hammer2_kthread_windows.c - Windows kthread / waitqueue backend.
 */

#include "hammer2.h"
#include "hammer2_kthread_windows.h"
#include <errno.h>
#include <stdarg.h>

/* Bounded poll so a lost wake can never hang an "infinite" wait_event(). */
#define H2_WQ_POLL_MS	100

void
h2_wq_wait(wait_queue_head_t *wq, int timeout_ms)
{
	DWORD ms;

	if (!wq->inited)
		init_waitqueue_head(wq);

	if (timeout_ms <= 0 || timeout_ms > H2_WQ_POLL_MS)
		ms = H2_WQ_POLL_MS;
	else
		ms = (DWORD)timeout_ms;

	EnterCriticalSection(&wq->lk);
	SleepConditionVariableCS(&wq->cv, &wq->lk, ms);
	LeaveCriticalSection(&wq->lk);
}

static DWORD WINAPI
h2_thread_trampoline(LPVOID arg)
{
	struct task_struct *k = (struct task_struct *)arg;
	return (DWORD)k->kt_fn(k->kt_arg);
}

struct task_struct *
kthread_run(int (*threadfn)(void *), void *data, const char *namefmt, ...)
{
	struct task_struct *k;

	(void)namefmt;
	k = (struct task_struct *)calloc(1, sizeof(*k));
	if (k == NULL)
		return ERR_PTR(-ENOMEM);
	k->kt_fn = threadfn;
	k->kt_arg = data;
	k->kt_handle = CreateThread(NULL, 0, h2_thread_trampoline, k, 0, NULL);
	if (k->kt_handle == NULL) {
		free(k);
		return ERR_PTR(-(int)GetLastError());
	}
	return k;
}

int
kthread_stop(struct task_struct *k)
{
	if (k == NULL || IS_ERR(k))
		return -EINVAL;
	if (k->kt_handle) {
		WaitForSingleObject(k->kt_handle, INFINITE);
		CloseHandle(k->kt_handle);
	}
	free(k);
	return 0;
}

int
kthread_should_stop(void)
{
	return 0;	/* HAMMER2 uses its own STOP-flag handshake. */
}

long
schedule_timeout_uninterruptible(long timo)
{
	Sleep((DWORD)(timo > 0 ? timo : 1));
	return 0;
}

void
panic(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	abort();
}

/* ---- DMSG file/socket I/O stubs (cluster path inactive for local mount) --- */

ssize_t
kernel_read(struct file *file, void *buf, size_t count, loff_t *pos)
{
	(void)file; (void)buf; (void)count; (void)pos;
	return -EIO;
}

ssize_t
kernel_write(struct file *file, const void *buf, size_t count, loff_t *pos)
{
	(void)file; (void)buf; (void)count; (void)pos;
	return -EIO;
}

struct socket *
sock_from_file(struct file *file)
{
	(void)file;
	return NULL;
}

int
kernel_sock_shutdown(struct socket *sock, int how)
{
	(void)sock; (void)how;
	return 0;
}

/*
 * hammer2_iget() built a Linux struct inode from a hammer2_inode_t in the old
 * linux_vfs.c.  The Windows driver (hammer2_windows_vfs.c) works directly with
 * hammer2_inode_t and never goes through struct inode, so the BSD-style
 * hammer2_igetv() path that calls this is unused; provide a stub so the core
 * links.  TODO: remove once hammer2_igetv() is dropped from the Windows build.
 */
struct inode *
hammer2_iget(struct super_block *sb, hammer2_inode_t *ip)
{
	(void)sb; (void)ip;
	return NULL;
}
