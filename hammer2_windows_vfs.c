/*
 * hammer2_windows_vfs.c - native Windows VFS driver for HAMMER2 (Dokan-based).
 *
 * Replaces the Linux-kernel binding (hammer2_linux_vfs.c).  It mounts a HAMMER2
 * volume through the unmodified core (hammer2_mount) over the Win32 block-device
 * backend (hammer2_device_windows.c), then exposes it to Windows as a drive
 * letter via the Dokan user-mode filesystem framework.
 *
 * Path resolution, directory enumeration and attribute lookup are driven
 * through HAMMER2's XOP cluster operations exactly as hammer2_vnops.c does on
 * BSD/Linux.  File reads use the chain layer directly (uncompressed + embedded
 * data); LZ4/ZLIB-compressed extents are a documented follow-up (the core's
 * decompress callbacks are static to hammer2_strategy.c).
 *
 * Build: links hammer2_core.lib + dokan1.lib.  Run:
 *     hammer2_mount <image-or-\\.\PhysicalDriveN> <X:>
 */

/*
 * NT status codes.  WIN32_LEAN_AND_MEAN (set by hammer2_windows_port.h) keeps
 * windows.h from defining the STATUS_* set, so use the documented
 * WIN32_NO_STATUS dance to let <ntstatus.h> own it, and provide the NTSTATUS
 * typedef that Dokan's headers require.  We deliberately do NOT include
 * <winternl.h> -- Dokan's fileinfo.h supplies UNICODE_STRING /
 * FILE_INFORMATION_CLASS itself and would clash with it.
 */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN	/* match the core build; avoids RPC uuid_t clash */
#endif
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>
typedef LONG NTSTATUS;
typedef NTSTATUS *PNTSTATUS;

#include "hammer2.h"

#include <dokan/dokan.h>
#include <dokan/fileinfo.h>

/* ---- Mounted state -------------------------------------------------------- */

static struct mount		*g_mp;
static hammer2_pfs_t		*g_pmp;
static hammer2_inode_t		*g_root;

/* Per-open-file context handed back through DOKAN_FILE_INFO->Context. */
typedef struct h2_open {
	hammer2_inode_t	*ip;
} h2_open_t;

static void h2_log(const char *fmt, ...);	/* lightweight env-gated tracing */

/* ---- UTF-16 <-> UTF-8 helpers --------------------------------------------- */

static int
wide_to_utf8(const WCHAR *w, char *out, int outsz)
{
	int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, out, outsz, NULL, NULL);
	return n > 0 ? n - 1 : -1;	/* bytes excluding NUL, or -1 */
}

static void
utf8_to_wide(const char *s, int slen, WCHAR *out, int outcch)
{
	int n = MultiByteToWideChar(CP_UTF8, 0, s, slen, out, outcch - 1);
	if (n < 0)
		n = 0;
	out[n] = L'\0';
}

/* ---- Core bridge ---------------------------------------------------------- */

/*
 * Resolve a single path component within directory `dip` via the nresolve XOP.
 * Returns a referenced+unlocked hammer2_inode_t, or NULL if not found.
 */
static hammer2_inode_t *
h2_lookup_one(hammer2_inode_t *dip, const char *name, size_t namelen)
{
	hammer2_xop_nresolve_t *xop;
	hammer2_inode_t *ip = NULL;
	int error;

	h2_log("  lookup1 dip=%p name='%.*s'\n", (void *)dip, (int)namelen, name);
	hammer2_inode_lock(dip, HAMMER2_RESOLVE_SHARED);
	xop = hammer2_xop_alloc(dip, 0);
	hammer2_xop_setname(&xop->head, name, namelen);
	hammer2_xop_start(&xop->head, &hammer2_nresolve_desc);
	error = hammer2_xop_collect(&xop->head, 0);
	error = hammer2_error_to_errno(error);
	h2_log("  lookup1 collect err=%d\n", error);
	if (error == 0) {
		/*
		 * hammer2_inode_get() returns the inode locked with a single ref
		 * that belongs to the lock (hammer2_inode_unlock() both unlocks and
		 * drops).  We want to hand the caller a referenced-but-unlocked
		 * inode, so take our own ref first, then unlock.
		 */
		ip = hammer2_inode_get(dip->pmp, &xop->head, -1, -1);
		h2_log("  lookup1 got ip=%p type=%u\n", (void *)ip,
		    ip ? (unsigned)ip->meta.type : 0);
		hammer2_inode_ref(ip);
		hammer2_inode_unlock(ip);
	}
	hammer2_xop_retire(&xop->head, HAMMER2_XOPMASK_VOP);
	hammer2_inode_unlock(dip);
	h2_log("  lookup1 done ip=%p\n", (void *)ip);
	return ip;
}

/*
 * Resolve a full Windows path ("\dir\sub\file") to a referenced inode.
 * The caller releases the result with hammer2_inode_drop().  "\" -> root.
 */
static hammer2_inode_t *
h2_resolve_path(const WCHAR *wpath)
{
	char path[HAMMER2_PATH_MAX];
	hammer2_inode_t *cur, *next;
	char *p, *seg;

	if (g_root == NULL)
		return NULL;
	if (wide_to_utf8(wpath, path, sizeof(path)) < 0)
		return NULL;

	cur = g_root;
	hammer2_inode_ref(cur);

	p = path;
	while (*p) {
		while (*p == '\\' || *p == '/')
			++p;
		if (*p == '\0')
			break;
		seg = p;
		while (*p && *p != '\\' && *p != '/')
			++p;
		next = h2_lookup_one(cur, seg, (size_t)(p - seg));
		hammer2_inode_drop(cur);
		if (next == NULL)
			return NULL;
		cur = next;
	}
	return cur;
}

static int
h2_is_dir(hammer2_inode_t *ip)
{
	return ip->meta.type == HAMMER2_OBJTYPE_DIRECTORY;
}

/* HAMMER2 stores microseconds since the Unix epoch; convert to Win32 FILETIME. */
static void
h2_time_to_filetime(uint64_t h2usec, FILETIME *ft)
{
	uint64_t t100ns = h2usec * 10ULL + 116444736000000000ULL;
	ft->dwLowDateTime = (DWORD)t100ns;
	ft->dwHighDateTime = (DWORD)(t100ns >> 32);
}

static void
h2_fill_by_handle(hammer2_inode_t *ip, BY_HANDLE_FILE_INFORMATION *bhfi)
{
	memset(bhfi, 0, sizeof(*bhfi));
	bhfi->dwFileAttributes = h2_is_dir(ip) ? FILE_ATTRIBUTE_DIRECTORY
					       : FILE_ATTRIBUTE_NORMAL;
	bhfi->nFileSizeHigh = (DWORD)(ip->meta.size >> 32);
	bhfi->nFileSizeLow = (DWORD)ip->meta.size;
	bhfi->nNumberOfLinks = ip->meta.nlinks ? (DWORD)ip->meta.nlinks : 1;
	bhfi->nFileIndexHigh = (DWORD)(ip->meta.inum >> 32);
	bhfi->nFileIndexLow = (DWORD)ip->meta.inum;
	h2_time_to_filetime(ip->meta.ctime, &bhfi->ftCreationTime);
	h2_time_to_filetime(ip->meta.atime, &bhfi->ftLastAccessTime);
	h2_time_to_filetime(ip->meta.mtime, &bhfi->ftLastWriteTime);
}

/*
 * Read up to `len` bytes at `off` from a regular file inode into `buf`.
 * Returns bytes read (0 at/after EOF), or -1 on error.  Handles embedded
 * (DIRECTDATA) inodes and uncompressed on-media extents.
 */
static int
h2_read_file(hammer2_inode_t *ip, uint64_t off, char *buf, uint32_t len)
{
	uint64_t fsize = ip->meta.size;
	uint32_t done = 0;

	if (off >= fsize)
		return 0;
	if (off + len > fsize)
		len = (uint32_t)(fsize - off);

	hammer2_inode_lock(ip, HAMMER2_RESOLVE_SHARED);

	while (done < len) {
		hammer2_key_t lbase = (off + done) & ~(hammer2_key_t)HAMMER2_PBUFMASK;
		uint32_t boff = (uint32_t)((off + done) - lbase);
		uint32_t bavail = HAMMER2_PBUFSIZE - boff;
		uint32_t want = len - done;
		hammer2_chain_t *parent, *chain;
		hammer2_key_t key_dummy;
		int error = 0;

		if (want > bavail)
			want = bavail;

		parent = hammer2_inode_chain(ip, 0,
		    HAMMER2_RESOLVE_ALWAYS | HAMMER2_RESOLVE_SHARED);
		chain = NULL;
		if (parent) {
			chain = hammer2_chain_lookup(&parent, &key_dummy,
			    lbase, lbase, &error,
			    HAMMER2_LOOKUP_ALWAYS | HAMMER2_LOOKUP_SHARED);
		}

		if (chain == NULL) {
			/* Sparse hole. */
			memset(buf + done, 0, want);
		} else if (chain->bref.type == HAMMER2_BREF_TYPE_INODE) {
			const hammer2_inode_data_t *ip2 =
			    &((const hammer2_media_data_t *)chain->data)->ipdata;
			uint32_t n = want;
			if (boff < HAMMER2_EMBEDDED_BYTES) {
				if (n > HAMMER2_EMBEDDED_BYTES - boff)
					n = HAMMER2_EMBEDDED_BYTES - boff;
				memcpy(buf + done, ip2->u.data + boff, n);
				if (n < want)
					memset(buf + done + n, 0, want - n);
			} else {
				memset(buf + done, 0, want);
			}
		} else if (chain->bref.type == HAMMER2_BREF_TYPE_DATA &&
			   HAMMER2_DEC_COMP(chain->bref.methods) == HAMMER2_COMP_NONE) {
			const char *data = (const char *)chain->data;
			uint32_t avail = chain->bytes > boff ? chain->bytes - boff : 0;
			uint32_t n = want < avail ? want : avail;
			if (n)
				memcpy(buf + done, data + boff, n);
			if (n < want)
				memset(buf + done + n, 0, want - n);
		} else {
			/*
			 * Compressed extent (LZ4/ZLIB).  The core's decompress
			 * callbacks are static to hammer2_strategy.c; exposing
			 * them is the remaining read-path bring-up item.
			 */
			if (chain) {
				hammer2_chain_unlock(chain);
				hammer2_chain_drop(chain);
			}
			if (parent) {
				hammer2_chain_unlock(parent);
				hammer2_chain_drop(parent);
			}
			hammer2_inode_unlock(ip);
			return done > 0 ? (int)done : -1;
		}

		if (chain) {
			hammer2_chain_unlock(chain);
			hammer2_chain_drop(chain);
		}
		if (parent) {
			hammer2_chain_unlock(parent);
			hammer2_chain_drop(parent);
		}
		done += want;
	}

	hammer2_inode_unlock(ip);
	return (int)done;
}

/* ---- Dokan operation callbacks -------------------------------------------- */

/* Lightweight tracing (set H2_TRACE=1 in the environment to enable). */
static int h2_trace = -1;
static void h2_log(const char *fmt, ...)
{
	va_list ap;
	if (h2_trace < 0) {
		char *e = getenv("H2_TRACE");
		h2_trace = (e && *e && *e != '0') ? 1 : 0;
	}
	if (!h2_trace)
		return;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fflush(stderr);
}

static NTSTATUS DOKAN_CALLBACK
h2_create_file(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext,
	       ACCESS_MASK DesiredAccess, ULONG FileAttributes,
	       ULONG ShareAccess, ULONG CreateDisposition,
	       ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo)
{
	hammer2_inode_t *ip;
	h2_open_t *oh;

	(void)SecurityContext; (void)DesiredAccess; (void)FileAttributes;
	(void)ShareAccess; (void)CreateOptions;

	/* Read-only filesystem: reject create/write dispositions. */
	if (CreateDisposition == FILE_CREATE ||
	    CreateDisposition == FILE_SUPERSEDE ||
	    CreateDisposition == FILE_OVERWRITE ||
	    CreateDisposition == FILE_OVERWRITE_IF)
		return STATUS_MEDIA_WRITE_PROTECTED;

	{
		char p8[260];
		wide_to_utf8(FileName, p8, sizeof(p8));
		h2_log("CREATE  '%s' disp=%lu\n", p8, (unsigned long)CreateDisposition);
	}

	ip = h2_resolve_path(FileName);
	if (ip == NULL) {
		h2_log("CREATE  -> NOT_FOUND\n");
		return STATUS_OBJECT_NAME_NOT_FOUND;
	}

	h2_log("CREATE  -> ip=%p type=%u size=%llu\n", (void *)ip,
	    (unsigned)ip->meta.type, (unsigned long long)ip->meta.size);
	DokanFileInfo->IsDirectory = h2_is_dir(ip) ? TRUE : FALSE;

	oh = (h2_open_t *)calloc(1, sizeof(*oh));
	if (oh == NULL) {
		hammer2_inode_drop(ip);
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	oh->ip = ip;	/* keeps the reference taken by h2_resolve_path */
	DokanFileInfo->Context = (ULONG64)(uintptr_t)oh;
	return STATUS_SUCCESS;
}

static void DOKAN_CALLBACK
h2_cleanup(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo)
{
	(void)FileName; (void)DokanFileInfo;
	h2_log("CLEANUP ctx=%p\n", (void *)(uintptr_t)DokanFileInfo->Context);
}

static void DOKAN_CALLBACK
h2_close_file(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo)
{
	h2_open_t *oh = (h2_open_t *)(uintptr_t)DokanFileInfo->Context;

	(void)FileName;
	h2_log("CLOSE   ctx=%p ip=%p\n", (void *)oh, oh ? (void *)oh->ip : NULL);
	if (oh) {
		if (oh->ip)
			hammer2_inode_drop(oh->ip);
		free(oh);
		DokanFileInfo->Context = 0;
	}
}

static NTSTATUS DOKAN_CALLBACK
h2_read_file_cb(LPCWSTR FileName, LPVOID Buffer, DWORD BufferLength,
		LPDWORD ReadLength, LONGLONG Offset,
		PDOKAN_FILE_INFO DokanFileInfo)
{
	h2_open_t *oh = (h2_open_t *)(uintptr_t)DokanFileInfo->Context;
	hammer2_inode_t *ip;
	int n;

	(void)FileName;
	if (oh == NULL || oh->ip == NULL)
		return STATUS_INVALID_HANDLE;
	ip = oh->ip;
	if (h2_is_dir(ip))
		return STATUS_FILE_IS_A_DIRECTORY;

	h2_log("READ    ip=%p off=%lld len=%lu size=%llu\n", (void *)ip,
	    (long long)Offset, (unsigned long)BufferLength,
	    (unsigned long long)ip->meta.size);
	n = h2_read_file(ip, (uint64_t)Offset, (char *)Buffer, BufferLength);
	h2_log("READ    -> %d\n", n);
	if (n < 0)
		return STATUS_IO_DEVICE_ERROR;
	*ReadLength = (DWORD)n;
	return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK
h2_get_file_info(LPCWSTR FileName, LPBY_HANDLE_FILE_INFORMATION Buffer,
		 PDOKAN_FILE_INFO DokanFileInfo)
{
	h2_open_t *oh = (h2_open_t *)(uintptr_t)DokanFileInfo->Context;

	(void)FileName;
	h2_log("GETINFO ctx=%p ip=%p\n", (void *)oh, oh ? (void *)oh->ip : NULL);
	if (oh == NULL || oh->ip == NULL)
		return STATUS_INVALID_HANDLE;
	h2_fill_by_handle(oh->ip, Buffer);
	return STATUS_SUCCESS;
}

/*
 * Enumerate a directory.  Emits "." / ".." then walks the readdir XOP exactly
 * as hammer2_vnops.c does, decoding INODE and DIRENT blockrefs.
 */
static NTSTATUS DOKAN_CALLBACK
h2_find_files(LPCWSTR FileName, PFillFindData FillFindData,
	      PDOKAN_FILE_INFO DokanFileInfo)
{
	h2_open_t *oh = (h2_open_t *)(uintptr_t)DokanFileInfo->Context;
	hammer2_inode_t *ip;
	hammer2_xop_readdir_t *xop;
	hammer2_blockref_t bref;
	WIN32_FIND_DATAW fd;
	int error;

	(void)FileName;
	if (oh == NULL || oh->ip == NULL)
		return STATUS_INVALID_HANDLE;
	ip = oh->ip;
	h2_log("FIND    ip=%p type=%u\n", (void *)ip, (unsigned)ip->meta.type);
	if (!h2_is_dir(ip))
		return STATUS_INVALID_PARAMETER;

	/*
	 * A valid (non-1601) FILETIME is required or the shell/`dir` rejects the
	 * entry with ERROR_INVALID_PARAMETER.  Use the directory's own mtime as
	 * the default and override per-entry where the on-disk inode carries one.
	 */
	FILETIME dirft;
	h2_time_to_filetime(ip->meta.mtime, &dirft);

	memset(&fd, 0, sizeof(fd));
	fd.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
	fd.ftCreationTime = fd.ftLastAccessTime = fd.ftLastWriteTime = dirft;
	wcscpy_s(fd.cFileName, MAX_PATH, L".");
	FillFindData(&fd, DokanFileInfo);
	wcscpy_s(fd.cFileName, MAX_PATH, L"..");
	FillFindData(&fd, DokanFileInfo);

	hammer2_inode_lock(ip, HAMMER2_RESOLVE_SHARED);
	xop = hammer2_xop_alloc(ip, 0);
	xop->lkey = 0 | HAMMER2_DIRHASH_VISIBLE;
	hammer2_xop_start(&xop->head, &hammer2_readdir_desc);

	for (;;) {
		const char *dname = NULL;
		uint16_t namlen = 0;
		uint8_t dtype = DT_UNKNOWN;
		uint64_t fsize = 0;

		error = hammer2_xop_collect(&xop->head, 0);
		error = hammer2_error_to_errno(error);
		if (error)
			break;
		hammer2_cluster_bref(&xop->head.cluster, &bref);

		if (bref.type == HAMMER2_BREF_TYPE_INODE) {
			const hammer2_inode_data_t *ripdata =
			    &((const hammer2_media_data_t *)
			      hammer2_xop_gdata(&xop->head))->ipdata;
			dtype = ripdata->meta.type;
			namlen = ripdata->meta.name_len;
			dname = ripdata->filename;
			fsize = ripdata->meta.size;
			memset(&fd, 0, sizeof(fd));
			fd.dwFileAttributes =
			    (dtype == HAMMER2_OBJTYPE_DIRECTORY)
			    ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
			fd.nFileSizeHigh = (DWORD)(fsize >> 32);
			fd.nFileSizeLow = (DWORD)fsize;
			h2_time_to_filetime(ripdata->meta.ctime, &fd.ftCreationTime);
			h2_time_to_filetime(ripdata->meta.atime, &fd.ftLastAccessTime);
			h2_time_to_filetime(ripdata->meta.mtime, &fd.ftLastWriteTime);
			utf8_to_wide(dname, namlen, fd.cFileName, MAX_PATH);
			FillFindData(&fd, DokanFileInfo);
			hammer2_xop_pdata(&xop->head);
		} else if (bref.type == HAMMER2_BREF_TYPE_DIRENT) {
			dtype = bref.embed.dirent.type;
			namlen = bref.embed.dirent.namlen;
			if (namlen <= sizeof(bref.check.buf))
				dname = bref.check.buf;
			else
				dname = ((const hammer2_media_data_t *)
				    hammer2_xop_gdata(&xop->head))->buf;
			memset(&fd, 0, sizeof(fd));
			fd.dwFileAttributes =
			    (dtype == HAMMER2_OBJTYPE_DIRECTORY)
			    ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
			/* DIRENT brefs carry no timestamps; inherit the dir's. */
			fd.ftCreationTime = fd.ftLastAccessTime =
			    fd.ftLastWriteTime = dirft;
			utf8_to_wide(dname, namlen, fd.cFileName, MAX_PATH);
			FillFindData(&fd, DokanFileInfo);
			if (namlen > sizeof(bref.check.buf))
				hammer2_xop_pdata(&xop->head);
		}
	}
	hammer2_xop_retire(&xop->head, HAMMER2_XOPMASK_VOP);
	hammer2_inode_unlock(ip);
	return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK
h2_get_volume_info(LPWSTR VolumeNameBuffer, DWORD VolumeNameSize,
		   LPDWORD VolumeSerialNumber, LPDWORD MaximumComponentLength,
		   LPDWORD FileSystemFlags, LPWSTR FileSystemNameBuffer,
		   DWORD FileSystemNameSize, PDOKAN_FILE_INFO DokanFileInfo)
{
	(void)DokanFileInfo;
	wcscpy_s(VolumeNameBuffer, VolumeNameSize, L"HAMMER2");
	if (VolumeSerialNumber)
		*VolumeSerialNumber = 0x48324653; /* "H2FS" */
	if (MaximumComponentLength)
		*MaximumComponentLength = 255;
	if (FileSystemFlags)
		*FileSystemFlags = FILE_CASE_SENSITIVE_SEARCH |
				   FILE_CASE_PRESERVED_NAMES |
				   FILE_UNICODE_ON_DISK | FILE_READ_ONLY_VOLUME;
	wcscpy_s(FileSystemNameBuffer, FileSystemNameSize, L"HAMMER2");
	return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK
h2_get_disk_free_space(PULONGLONG FreeBytesAvailable,
		       PULONGLONG TotalNumberOfBytes,
		       PULONGLONG TotalNumberOfFreeBytes,
		       PDOKAN_FILE_INFO DokanFileInfo)
{
	struct h2statfs sb;

	(void)DokanFileInfo;
	memset(&sb, 0, sizeof(sb));
	if (g_mp)
		hammer2_statfs(g_mp, &sb);

	if (TotalNumberOfBytes)
		*TotalNumberOfBytes = sb.f_blocks * (ULONGLONG)(sb.f_bsize ? sb.f_bsize : 1);
	if (FreeBytesAvailable)
		*FreeBytesAvailable = sb.f_bavail * (ULONGLONG)(sb.f_bsize ? sb.f_bsize : 1);
	if (TotalNumberOfFreeBytes)
		*TotalNumberOfFreeBytes = sb.f_bfree * (ULONGLONG)(sb.f_bsize ? sb.f_bsize : 1);
	return STATUS_SUCCESS;
}

/* ---- Mount / main --------------------------------------------------------- */

static int
h2_mount_volume(const char *device)
{
	struct mount *mp;
	struct h2_mount_optlist *ol;
	char *devstr;
	static int hflags;

	hammer2_init(NULL);	/* zones + global lock + lists */

	mp = (struct mount *)kzalloc(sizeof(*mp), 0);
	ol = (struct h2_mount_optlist *)kzalloc(sizeof(*ol), 0);
	devstr = _strdup(device);
	if (!mp || !ol || !devstr)
		return -1;

	ol->count = 3;
	ol->opts[0].name = "from";
	ol->opts[0].value = devstr;
	ol->opts[0].len = (int)strlen(devstr) + 1;
	ol->opts[1].name = "fspath";
	ol->opts[1].value = devstr;
	ol->opts[1].len = (int)strlen(devstr) + 1;
	ol->opts[2].name = "hflags";
	ol->opts[2].value = &hflags;
	ol->opts[2].len = sizeof(int);

	mp->mnt_optnew = ol;
	mp->mnt_flag = MNT_RDONLY;
	mp->mnt_iosize_max = MAXPHYS;

	if (hammer2_mount(mp) != 0) {
		fprintf(stderr, "hammer2: mount of '%s' failed\n", device);
		return -1;
	}
	g_mp = mp;
	g_pmp = (hammer2_pfs_t *)mp->mnt_data;
	if (g_pmp == NULL || g_pmp->iroot == NULL) {
		fprintf(stderr, "hammer2: no root inode after mount\n");
		return -1;
	}
	g_root = g_pmp->iroot;
	return 0;
}

int
main(int argc, char **argv)
{
	DOKAN_OPTIONS options;
	DOKAN_OPERATIONS ops;
	WCHAR mountpoint[8];
	int status;

	/* Inspection subcommands (pfs-list, volume-list) live in hammer2.exe. */
	if (argc < 3) {
		fprintf(stderr,
		    "usage: %s <image-or-\\\\.\\PhysicalDriveN> <X:>\n",
		    argv[0]);
		return 2;
	}

	if (h2_mount_volume(argv[1]) != 0)
		return 1;
	fprintf(stderr, "hammer2: mounted '%s', serving on %s\n", argv[1], argv[2]);

	utf8_to_wide(argv[2], -1, mountpoint, 8);

	memset(&options, 0, sizeof(options));
	options.Version = DOKAN_VERSION;
	options.ThreadCount = 1;	/* core XOP path is not yet MT-validated */
	options.MountPoint = mountpoint;
	options.Options = DOKAN_OPTION_WRITE_PROTECT;

	memset(&ops, 0, sizeof(ops));
	ops.ZwCreateFile = h2_create_file;
	ops.Cleanup = h2_cleanup;
	ops.CloseFile = h2_close_file;
	ops.ReadFile = h2_read_file_cb;
	ops.GetFileInformation = h2_get_file_info;
	ops.FindFiles = h2_find_files;
	ops.GetVolumeInformation = h2_get_volume_info;
	ops.GetDiskFreeSpace = h2_get_disk_free_space;

	status = DokanMain(&options, &ops);
	fprintf(stderr, "hammer2: DokanMain returned %d\n", status);

	if (g_mp)
		hammer2_unmount(g_mp, 0);
	hammer2_uninit(NULL);
	return status == DOKAN_SUCCESS ? 0 : 1;
}
