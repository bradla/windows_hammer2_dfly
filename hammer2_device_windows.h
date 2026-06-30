/*
 * hammer2_device_windows.h - Windows block-device backend for HAMMER2.
 *
 * The HAMMER2 device I/O path (hammer2_io.c / hammer2_ondisk.c) is written
 * against the Linux block-device + buffer-cache API:
 *   bdev_file_open_by_path() / file_bdev() / fput()
 *   __bread() / __getblk() / mark_buffer_dirty() / sync_dirty_buffer() / brelse()
 *   bdev_nr_bytes() / IS_ERR() / PTR_ERR()
 *
 * This header re-implements exactly that surface over a Win32 file HANDLE, so
 * the unmodified core reads and writes a real HAMMER2 volume (an image file or
 * a \\.\PhysicalDriveN device).
 */

#ifndef _HAMMER2_DEVICE_WINDOWS_H_
#define _HAMMER2_DEVICE_WINDOWS_H_

#include "hammer2_windows_port.h"

/*
 * struct block_device / struct file are forward-declared in
 * hammer2_windows_port.h; define them here as the concrete Windows backend.
 */
struct block_device {
	HANDLE		bd_handle;	/* Win32 file/device handle */
	uint64_t	bd_size;	/* media size in bytes */
	uint32_t	bd_blocksize;	/* logical block size (default 512) */
};

struct file {
	struct block_device	bdev;	/* embedded; file_bdev() returns &bdev */
	int			f_flags;
};

/*
 * Linux ERR_PTR / IS_ERR encode small negative errnos in the pointer value.
 */
#define HAMMER2_ERR_PTR_MAX	4095UL
static inline void *ERR_PTR(long err)		{ return (void *)(intptr_t)err; }
static inline long  PTR_ERR(const void *ptr)	{ return (long)(intptr_t)ptr; }
static inline int   IS_ERR(const void *ptr)
{
	return (uintptr_t)ptr >= (uintptr_t)(-(intptr_t)HAMMER2_ERR_PTR_MAX);
}
static inline int   IS_ERR_OR_NULL(const void *ptr)
{
	return !ptr || IS_ERR(ptr);
}

struct file *bdev_file_open_by_path(const char *path, int flags,
				    void *holder, const void *hops);
struct block_device *file_bdev(struct file *bdev_file);
void fput(struct file *file);
struct file *fget(unsigned int fd);

uint64_t bdev_nr_bytes(struct block_device *bdev);
static inline uint64_t bdev_nr_sectors(struct block_device *bdev)
{
	return bdev_nr_bytes(bdev) / 512;
}
int set_blocksize(struct block_device *bdev, int size);
int sync_blockdev(struct block_device *bdev);

/* Buffer-cache API: __bread/__getblk return a buffer_head whose b_data holds
 * the block.  Each is independently allocated and freed by brelse(). */
struct buffer_head *__bread(struct block_device *bdev, sector_t block,
			    unsigned int size);
struct buffer_head *__getblk(struct block_device *bdev, sector_t block,
			     unsigned int size);
void mark_buffer_dirty(struct buffer_head *bh);
int  sync_dirty_buffer(struct buffer_head *bh);
void brelse(struct buffer_head *bh);
void bforget(struct buffer_head *bh);

/* Buffer state helpers -- no-ops for the single-threaded image backend. */
#define lock_buffer(bh)		((void)(bh))
#define unlock_buffer(bh)	((void)(bh))
#define set_buffer_uptodate(bh)	((void)(bh))
#define buffer_uptodate(bh)	(1)
#define get_bh(bh)		((void)(bh))
#define put_bh(bh)		((void)(bh))

#endif /* _HAMMER2_DEVICE_WINDOWS_H_ */
