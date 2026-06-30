/*
 * hammer2_user_windows.h - lightweight userland compatibility shim for the
 * HAMMER2 offline tools (newfs_hammer2 / fsck_hammer2) on Windows/MSVC.
 *
 * Unlike hammer2_windows_port.h (the kernel/driver shim) this header pulls in
 * only what the standalone tools need: fixed-width types, the on-disk
 * `struct uuid`, GCC attribute shims, and POSIX file-I/O / getopt / err
 * wrappers over the Win32 CRT.  It deliberately does NOT define uuid_t (the
 * libuuid type) -- that comes from the <uuid/uuid.h> shim.
 */

#ifndef _HAMMER2_USER_WINDOWS_H_
#define _HAMMER2_USER_WINDOWS_H_

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>

/* Linux/BSD fixed-width integer aliases used by the on-disk format. */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef unsigned int   u_int;
typedef unsigned long  u_long;
typedef unsigned char  u_char;
typedef unsigned short u_short;
typedef uint8_t  u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
typedef uint64_t u_int64_t;

#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef long long ssize_t;
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#ifndef NAME_MAX
#define NAME_MAX 255
#endif

/* On-disk BSD UUID (16 bytes), matching DragonFly's struct uuid. */
struct uuid {
	uint32_t	time_low;
	uint16_t	time_mid;
	uint16_t	time_hi_and_version;
	uint8_t		clock_seq_hi_and_reserved;
	uint8_t		clock_seq_low;
	uint8_t		node[6];
};

/*
 * libuuid type + the HAMMER2 wrapper.  Defined here (rather than only in the
 * <uuid/uuid.h> shim) because hammer2_disk.h references hammer2_uuid_t and is
 * sometimes included before <uuid/uuid.h>.
 */
#ifndef _H2U_UUID_DEFINED
#define _H2U_UUID_DEFINED
typedef unsigned char uuid_t[16];
typedef struct hammer2_uuid { uuid_t uuid; } hammer2_uuid_t;
void uuid_generate(uuid_t out);
int  uuid_parse(const char *in, uuid_t uu);
void uuid_unparse(const uuid_t uu, char *out);
int  uuid_compare(const uuid_t a, const uuid_t b);
#endif

/* GCC/Clang attribute shims for MSVC. */
#ifndef __packed
#define __packed
#endif
#ifndef __unused
#define __unused
#endif
#ifndef __aligned
#define __aligned(x)
#endif
#ifndef __attribute__
#define __attribute__(x)
#endif

/* BSD bzero/bcopy. */
#ifndef bzero
#define bzero(p, n)	memset((p), 0, (n))
#endif
#ifndef bcopy
#define bcopy(s, d, n)	memmove((d), (s), (n))
#endif

/* POSIX file I/O over the Win32 CRT (<io.h>). */
#ifndef O_RDONLY
#define O_RDONLY	_O_RDONLY
#define O_WRONLY	_O_WRONLY
#define O_RDWR		_O_RDWR
#define O_CREAT		_O_CREAT
#define O_TRUNC		_O_TRUNC
#define O_BINARY	_O_BINARY
#endif

static __inline int
h2u_open(const char *path, int flags)
{
	return _open(path, flags | _O_BINARY, _S_IREAD | _S_IWRITE);
}
#define open(path, flags, ...)	h2u_open((path), (flags))
#define close(fd)		_close(fd)
#define read(fd, buf, n)	_read((fd), (buf), (unsigned)(n))
#define write(fd, buf, n)	_write((fd), (buf), (unsigned)(n))
#define lseek(fd, off, w)	_lseeki64((fd), (off), (w))
#define ftruncate(fd, len)	_chsize_s((fd), (len))
#define fsync(fd)		_commit(fd)

/* pwrite/pread: seek + write/read (single-threaded tools, so this is fine). */
static __inline ssize_t
pwrite(int fd, const void *buf, size_t n, long long off)
{
	if (_lseeki64(fd, off, SEEK_SET) < 0)
		return -1;
	return _write(fd, buf, (unsigned)n);
}
static __inline ssize_t
pread(int fd, void *buf, size_t n, long long off)
{
	if (_lseeki64(fd, off, SEEK_SET) < 0)
		return -1;
	return _read(fd, buf, (unsigned)n);
}

#ifndef S_ISREG
#define S_ISREG(m)	(((m) & _S_IFMT) == _S_IFREG)
#endif
#ifndef S_ISCHR
#define S_ISCHR(m)	(((m) & _S_IFMT) == _S_IFCHR)
#endif
#ifndef S_ISBLK
#define S_ISBLK(m)	0	/* no block-special file type on Windows */
#endif
#ifndef S_ISDIR
#define S_ISDIR(m)	(((m) & _S_IFMT) == _S_IFDIR)
#endif

/* err(3) family.  Prefix with the actual program basename (not a hardcoded
 * tool name), like the BSD err(3) does. */
#include <stdarg.h>
static __inline const char *
h2u_progname(void)
{
	const char *p = _pgmptr ? _pgmptr : "hammer2";
	const char *s = strrchr(p, '\\');
	const char *t = strrchr(p, '/');
	if (t > s) s = t;
	return s ? s + 1 : p;
}
static __inline void
h2u_verr(int eval, int doerrno, const char *fmt, va_list ap)
{
	fprintf(stderr, "%s: ", h2u_progname());
	if (fmt)
		vfprintf(stderr, fmt, ap);
	if (doerrno)
		fprintf(stderr, ": errno %d", errno);
	fprintf(stderr, "\n");
	exit(eval);
}
static __inline void err(int eval, const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); h2u_verr(eval, 1, fmt, ap); va_end(ap); }
static __inline void errx(int eval, const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); h2u_verr(eval, 0, fmt, ap); va_end(ap); }
static __inline void warn(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); fprintf(stderr, "%s: ", h2u_progname());
  if (fmt) vfprintf(stderr, fmt, ap); fprintf(stderr, ": errno %d\n", errno); va_end(ap); }
static __inline void warnx(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); fprintf(stderr, "%s: ", h2u_progname());
  if (fmt) vfprintf(stderr, fmt, ap); fprintf(stderr, "\n"); va_end(ap); }

/* getopt(3): provided by hammer2_user_subs.c. */
extern char *optarg;
extern int optind, opterr, optopt;
int getopt(int argc, char * const argv[], const char *optstring);

/* Misc POSIX bits. */
#ifndef getpagesize
#define getpagesize()	4096
#endif
#ifndef srandom
#define srandom(s)	srand(s)
#define random()	rand()
#endif

/* DMSG peer type used in the volume header (from hammer2_dmsg.h). */
#ifndef DMSG_PEER_HAMMER2
#define DMSG_PEER_HAMMER2	3
#endif
#ifndef strcasecmp
#define strcasecmp	_stricmp
#endif
#ifndef strncasecmp
#define strncasecmp	_strnicmp
#endif

#endif /* _HAMMER2_USER_WINDOWS_H_ */
