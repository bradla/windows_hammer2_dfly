/*
 * hammer2_device_windows.c - Windows block-device backend implementation.
 *
 * Implements the Linux block-device + buffer-cache API surface that the
 * HAMMER2 core (hammer2_io.c / hammer2_ondisk.c) calls, backed by a Win32
 * file HANDLE.  Works on a HAMMER2 image file or a \\.\PhysicalDriveN device.
 */

#include "hammer2.h"
#include "hammer2_device_windows.h"
#include <winioctl.h>
#include <errno.h>

/*
 * Open a HAMMER2 backing store.  The core prepends "/dev/" to device names that
 * do not begin with '/', so we strip a leading "/dev/" and open the remainder
 * as a Win32 path (image file or \\.\PhysicalDriveN).
 */
struct file *
bdev_file_open_by_path(const char *path, int flags, void *holder,
		       const void *hops)
{
	struct file *f;
	HANDLE h;
	DWORD access, share;
	LARGE_INTEGER li;

	(void)holder;
	(void)hops;

	if (path == NULL)
		return ERR_PTR(-EINVAL);
	if (strncmp(path, "/dev/", 5) == 0)
		path += 5;

	access = GENERIC_READ;
	if (flags & BLK_OPEN_WRITE)
		access |= GENERIC_WRITE;
	share = FILE_SHARE_READ | FILE_SHARE_WRITE;

	h = CreateFileA(path, access, share, NULL, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE)
		return ERR_PTR(-(int)GetLastError());

	f = (struct file *)calloc(1, sizeof(*f));
	if (f == NULL) {
		CloseHandle(h);
		return ERR_PTR(-ENOMEM);
	}
	f->f_flags = flags;
	f->bdev.bd_handle = h;
	f->bdev.bd_blocksize = 512;

	/* Media size: file size, or disk length for a raw device. */
	if (GetFileSizeEx(h, &li) && li.QuadPart > 0) {
		f->bdev.bd_size = (uint64_t)li.QuadPart;
	} else {
		GET_LENGTH_INFORMATION gli;
		DWORD ret = 0;
		if (DeviceIoControl(h, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0,
				    &gli, sizeof(gli), &ret, NULL))
			f->bdev.bd_size = (uint64_t)gli.Length.QuadPart;
		else
			f->bdev.bd_size = 0;
	}
	return f;
}

struct block_device *
file_bdev(struct file *bdev_file)
{
	return &bdev_file->bdev;
}

void
fput(struct file *file)
{
	if (file == NULL || IS_ERR(file))
		return;
	if (file->bdev.bd_handle && file->bdev.bd_handle != INVALID_HANDLE_VALUE)
		CloseHandle(file->bdev.bd_handle);
	free(file);
}

struct file *
fget(unsigned int fd)
{
	(void)fd;
	return NULL;	/* not used by the image backend */
}

uint64_t
bdev_nr_bytes(struct block_device *bdev)
{
	return bdev ? bdev->bd_size : 0;
}

int
set_blocksize(struct block_device *bdev, int size)
{
	if (bdev)
		bdev->bd_blocksize = (uint32_t)size;
	return 0;
}

int
sync_blockdev(struct block_device *bdev)
{
	if (bdev && bdev->bd_handle)
		FlushFileBuffers(bdev->bd_handle);
	return 0;
}

/*
 * Seek+read/write helpers (absolute byte offset).
 */
static int
dev_pread(struct block_device *bdev, uint64_t off, void *buf, uint32_t len)
{
	LARGE_INTEGER li;
	DWORD got = 0;

	li.QuadPart = (LONGLONG)off;
	if (!SetFilePointerEx(bdev->bd_handle, li, NULL, FILE_BEGIN))
		return -EIO;
	if (!ReadFile(bdev->bd_handle, buf, len, &got, NULL))
		return -EIO;
	if (got < len)	/* short read past EOF -> zero-fill tail */
		memset((char *)buf + got, 0, len - got);
	return 0;
}

static int
dev_pwrite(struct block_device *bdev, uint64_t off, const void *buf,
	   uint32_t len)
{
	LARGE_INTEGER li;
	DWORD put = 0;

	li.QuadPart = (LONGLONG)off;
	if (!SetFilePointerEx(bdev->bd_handle, li, NULL, FILE_BEGIN))
		return -EIO;
	if (!WriteFile(bdev->bd_handle, buf, len, &put, NULL) || put != len)
		return -EIO;
	return 0;
}

static struct buffer_head *
alloc_bh(struct block_device *bdev, sector_t block, unsigned int size)
{
	struct buffer_head *bh = (struct buffer_head *)calloc(1, sizeof(*bh));
	if (bh == NULL)
		return NULL;
	bh->b_data = (char *)malloc(size);
	if (bh->b_data == NULL) {
		free(bh);
		return NULL;
	}
	bh->b_size = size;
	bh->b_blocknr = block;
	bh->b_bdev = bdev;
	bh->b_offset = (loff_t)((uint64_t)block * size);
	return bh;
}

struct buffer_head *
__bread(struct block_device *bdev, sector_t block, unsigned int size)
{
	struct buffer_head *bh = alloc_bh(bdev, block, size);
	if (bh == NULL)
		return NULL;
	if (dev_pread(bdev, (uint64_t)bh->b_offset, bh->b_data, size) != 0) {
		free(bh->b_data);
		free(bh);
		return NULL;
	}
	return bh;
}

struct buffer_head *
__getblk(struct block_device *bdev, sector_t block, unsigned int size)
{
	/* Like __bread but the caller overwrites the whole buffer; start zeroed. */
	struct buffer_head *bh = alloc_bh(bdev, block, size);
	if (bh)
		memset(bh->b_data, 0, size);
	return bh;
}

void
mark_buffer_dirty(struct buffer_head *bh)
{
	(void)bh;	/* write-through: sync_dirty_buffer() does the work */
}

int
sync_dirty_buffer(struct buffer_head *bh)
{
	if (bh == NULL || bh->b_bdev == NULL)
		return -EIO;
	return dev_pwrite(bh->b_bdev, (uint64_t)bh->b_offset, bh->b_data,
			  (uint32_t)bh->b_size);
}

void
brelse(struct buffer_head *bh)
{
	if (bh == NULL)
		return;
	free(bh->b_data);
	free(bh);
}

void
bforget(struct buffer_head *bh)
{
	brelse(bh);
}
