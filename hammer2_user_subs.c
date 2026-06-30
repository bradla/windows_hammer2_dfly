/*
 * hammer2_user_subs.c - userland support functions for the Windows port of
 * newfs_hammer2 / fsck_hammer2.  Provides the POSIX/BSD helpers the tools
 * expect (getopt, err already inline, iSCSI CRC32C, uuid, dirhash, sizes,
 * check_volume) that aren't part of the Win32 CRT.
 *
 * The CRC32C / dirhash / size-string logic mirrors lh1/src/sbin/hammer2/subs.c.
 */

<<<<<<< HEAD
#define HAMMER2_USERLAND
#include "hammer2_user_windows.h"
=======
#ifndef HAMMER2_USERLAND
#define HAMMER2_USERLAND		/* normally set by the build; harmless here */
#endif
#include "hammer2_user_windows.h"
/*
 * Declare CoCreateGuid() ourselves (ole32) rather than including <objbase.h> --
 * that header drags in the Windows COM `uuid_t` (the UUID struct), which
 * collides with the port's uuid_t (unsigned char[16]).  The explicit __stdcall
 * prototype gives the correct decorated name so it links on x86 too (on x64
 * there is only one calling convention, so it linked there regardless).
 */
HRESULT __stdcall CoCreateGuid(GUID *pguid);
>>>>>>> 21de6d5 (file cleanup)
#include <vfs/hammer2/hammer2_disk.h>
#include <vfs/hammer2/hammer2_xxhash.h>
#include <uuid/uuid.h>
#include "hammer2_subs.h"
#include <winioctl.h>
#include <time.h>

/* ---- getopt(3) ------------------------------------------------------------ */

char *optarg = NULL;
int optind = 1, opterr = 1, optopt = 0;

int
getopt(int argc, char *const argv[], const char *optstring)
{
	static int sp = 1;
	int c;
	const char *cp;

	if (sp == 1) {
		if (optind >= argc || argv[optind][0] != '-' ||
		    argv[optind][1] == '\0')
			return -1;
		if (strcmp(argv[optind], "--") == 0) {
			optind++;
			return -1;
		}
	}
	optopt = c = (unsigned char)argv[optind][sp];
	cp = strchr(optstring, c);
	if (c == ':' || cp == NULL) {
		if (argv[optind][++sp] == '\0') {
			optind++;
			sp = 1;
		}
		if (opterr)
			fprintf(stderr, "%s: illegal option -- %c\n", argv[0], c);
		return '?';
	}
	if (*++cp == ':') {
		if (argv[optind][sp + 1] != '\0') {
			optarg = &argv[optind++][sp + 1];
		} else if (++optind >= argc) {
			if (opterr)
				fprintf(stderr,
				    "%s: option requires an argument -- %c\n",
				    argv[0], c);
			sp = 1;
			return '?';
		} else {
			optarg = argv[optind++];
		}
		sp = 1;
	} else {
		if (argv[optind][++sp] == '\0') {
			sp = 1;
			optind++;
		}
		optarg = NULL;
	}
	return c;
}

/* ---- iSCSI CRC32C (Castagnoli) -------------------------------------------- */

static uint32_t
crc32c_raw(uint32_t crc, const void *buf, size_t len)
{
	const uint8_t *p = (const uint8_t *)buf;
	size_t i;
	int k;
	for (i = 0; i < len; ++i) {
		crc ^= p[i];
		for (k = 0; k < 8; ++k)
			crc = (crc >> 1) ^ (0x82F63B78U & (~(crc & 1) + 1));
	}
	return crc;
}

uint32_t
iscsi_crc32(const void *buf, size_t size)
{
	return ~crc32c_raw(~0U, buf, size);
}

uint32_t
iscsi_crc32_ext(const void *buf, size_t size, uint32_t ocrc)
{
	return ~crc32c_raw(~ocrc, buf, size);
}

/* ---- libuuid shims over Win32 GUID ---------------------------------------- */

void
uuid_generate(uuid_t out)
{
	GUID g;
	CoCreateGuid(&g);
	memcpy(out, &g, 16);
}

static int
hexval(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

int
uuid_parse(const char *in, uuid_t uu)
{
	int i, hi, lo;
	const char *p = in;
	for (i = 0; i < 16; ++i) {
		while (*p == '-')
			p++;
		hi = hexval(p[0]);
		lo = (hi >= 0) ? hexval(p[1]) : -1;
		if (hi < 0 || lo < 0)
			return -1;
		uu[i] = (unsigned char)((hi << 4) | lo);
		p += 2;
	}
	return 0;
}

void
uuid_unparse(const uuid_t uu, char *out)
{
	snprintf(out, 37,
	    "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
	    uu[0], uu[1], uu[2], uu[3], uu[4], uu[5], uu[6], uu[7],
	    uu[8], uu[9], uu[10], uu[11], uu[12], uu[13], uu[14], uu[15]);
}

int
uuid_compare(const uuid_t a, const uuid_t b)
{
	return memcmp(a, b, 16);
}

/* ---- hammer2 uuid wrappers (used by mkfs) --------------------------------- */

void
hammer2_uuid_create(hammer2_uuid_t *uuid)
{
	uuid_generate(uuid->uuid);
}

int
hammer2_uuid_from_string(const char *str, hammer2_uuid_t *uuid)
{
	return uuid_parse(str, uuid->uuid) ? -1 : 0;
}

const char *
hammer2_uuid_to_str(const hammer2_uuid_t *uuid, char **strp)
{
	/*
	 * The on-disk uuid is a BSD `struct uuid` (time_low/mid/hi are native
	 * integers).  Format it field-wise -- the canonical textual form, and
	 * the same representation the kernel ioctl path prints -- rather than as
	 * a raw byte dump (which would byte-swap the first three fields).
	 */
	const struct uuid *u = (const struct uuid *)uuid;

	if (*strp)
		free(*strp);
	*strp = (char *)malloc(40);
	snprintf(*strp, 40,
	    "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
	    u->time_low, u->time_mid, u->time_hi_and_version,
	    u->clock_seq_hi_and_reserved, u->clock_seq_low,
	    u->node[0], u->node[1], u->node[2],
	    u->node[3], u->node[4], u->node[5]);
	return *strp;
}

int
hammer2_uuid_compare(const hammer2_uuid_t *u1, const hammer2_uuid_t *u2)
{
	return memcmp(u1, u2, sizeof(*u1));
}

/*
 * getdevpath(): BSD resolves a device label to /dev/<path>.  On Windows the
 * caller already passes a usable Win32 path (image file or \\.\PhysicalDriveN),
 * so just hand back a copy.
 */
char *
getdevpath(const char *devname, int flags)
{
	(void)flags;
	return _strdup(devname);
}

/* ---- on-disk enum -> string (mirrors lh1/src/sbin/hammer2/subs.c) --------- */

const char *
hammer2_time64_to_str(uint64_t htime64, char **strp)
{
	struct tm *tp;
	time_t t;

	if (*strp) {
		free(*strp);
		*strp = NULL;
	}
	*strp = (char *)malloc(64);
	t = (time_t)(htime64 / 1000000);
	tp = localtime(&t);
	strftime(*strp, 64, "%d-%b-%Y %H:%M:%S", tp);
	return *strp;
}

const char *
hammer2_iptype_to_str(uint8_t type)
{
	switch (type) {
	case HAMMER2_OBJTYPE_UNKNOWN:	return "UNKNOWN";
	case HAMMER2_OBJTYPE_DIRECTORY:	return "DIR";
	case HAMMER2_OBJTYPE_REGFILE:	return "FILE";
	case HAMMER2_OBJTYPE_FIFO:	return "FIFO";
	case HAMMER2_OBJTYPE_CDEV:	return "CDEV";
	case HAMMER2_OBJTYPE_BDEV:	return "BDEV";
	case HAMMER2_OBJTYPE_SOFTLINK:	return "SOFTLINK";
	case HAMMER2_OBJTYPE_SOCKET:	return "SOCKET";
	case HAMMER2_OBJTYPE_WHITEOUT:	return "WHITEOUT";
	default:			return "ILLEGAL";
	}
}

const char *
hammer2_pfstype_to_str(uint8_t type)
{
	switch (type) {
	case HAMMER2_PFSTYPE_NONE:		return "NONE";
	case HAMMER2_PFSTYPE_SUPROOT:		return "SUPROOT";
	case HAMMER2_PFSTYPE_DUMMY:		return "DUMMY";
	case HAMMER2_PFSTYPE_CACHE:		return "CACHE";
	case HAMMER2_PFSTYPE_SLAVE:		return "SLAVE";
	case HAMMER2_PFSTYPE_SOFT_SLAVE:	return "SOFT_SLAVE";
	case HAMMER2_PFSTYPE_SOFT_MASTER:	return "SOFT_MASTER";
	case HAMMER2_PFSTYPE_MASTER:		return "MASTER";
	default:				return "ILLEGAL";
	}
}

const char *
hammer2_pfssubtype_to_str(uint8_t subtype)
{
	switch (subtype) {
	case HAMMER2_PFSSUBTYPE_NONE:		return "NONE";
	case HAMMER2_PFSSUBTYPE_SNAPSHOT:	return "SNAPSHOT";
	case HAMMER2_PFSSUBTYPE_AUTOSNAP:	return "AUTOSNAP";
	default:				return "ILLEGAL";
	}
}

const char *
hammer2_breftype_to_str(uint8_t type)
{
	switch (type) {
	case HAMMER2_BREF_TYPE_EMPTY:		return "empty";
	case HAMMER2_BREF_TYPE_INODE:		return "inode";
	case HAMMER2_BREF_TYPE_INDIRECT:	return "indirect";
	case HAMMER2_BREF_TYPE_DATA:		return "data";
	case HAMMER2_BREF_TYPE_DIRENT:		return "dirent";
	case HAMMER2_BREF_TYPE_FREEMAP_NODE:	return "freemap_node";
	case HAMMER2_BREF_TYPE_FREEMAP_LEAF:	return "freemap_leaf";
	case HAMMER2_BREF_TYPE_INVALID:		return "invalid";
	case HAMMER2_BREF_TYPE_FREEMAP:		return "freemap";
	case HAMMER2_BREF_TYPE_VOLUME:		return "volume";
	default:				return "unknown";
	}
}

/* ---- size / count string formatting --------------------------------------- */

const char *
sizetostr(hammer2_off_t size)
{
	static char buf[32];

	if (size < 1024 / 2)
		snprintf(buf, sizeof(buf), "%6.2fB", (double)size);
	else if (size < 1024 * 1024 / 2)
		snprintf(buf, sizeof(buf), "%6.2fKB", (double)size / 1024);
	else if (size < 1024 * 1024 * 1024LL / 2)
		snprintf(buf, sizeof(buf), "%6.2fMB", (double)size / (1024 * 1024));
	else if (size < 1024 * 1024 * 1024LL * 1024LL / 2)
		snprintf(buf, sizeof(buf), "%6.2fGB",
		    (double)size / (1024 * 1024 * 1024LL));
	else
		snprintf(buf, sizeof(buf), "%6.2fTB",
		    (double)size / (1024 * 1024 * 1024LL * 1024LL));
	return buf;
}

const char *
counttostr(hammer2_off_t size)
{
	static char buf[32];
	snprintf(buf, sizeof(buf), "%lld", (long long)size);
	return buf;
}

/* getsize() is provided by mkfs_hammer2.c. */

/* ---- dirhash (mirrors subs.c) --------------------------------------------- */

hammer2_key_t
dirhash(const char *aname, size_t len)
{
	uint32_t crcx;
	uint64_t key = 0;
	size_t i, j;

	crcx = 0;
	for (i = j = 0; i < len; ++i) {
		if (aname[i] == '.' || aname[i] == '-' ||
		    aname[i] == '_' || aname[i] == '~') {
			if (i != j)
				crcx += hammer2_icrc32(aname + j, i - j);
			j = i + 1;
		}
	}
	if (i != j)
		crcx += hammer2_icrc32(aname + j, i - j);

	crcx |= 0x80000000U;
	key |= (uint64_t)crcx << 32;

	crcx = hammer2_icrc32(aname, len);
	crcx = crcx ^ (crcx << 16);
	key |= crcx & 0xFFFF0000U;
	key |= 0x8000U;

	return (hammer2_key_t)key;
}

/* ---- check_volume: media size of an image file or raw device -------------- */

hammer2_off_t
check_volume(int fd)
{
	int64_t size;
	HANDLE h = (HANDLE)_get_osfhandle(fd);
	LARGE_INTEGER li;

	if (h != INVALID_HANDLE_VALUE && GetFileSizeEx(h, &li) && li.QuadPart > 0)
		return (hammer2_off_t)li.QuadPart;

	if (h != INVALID_HANDLE_VALUE) {
		GET_LENGTH_INFORMATION gli;
		DWORD ret = 0;
		if (DeviceIoControl(h, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0,
		    &gli, sizeof(gli), &ret, NULL))
			return (hammer2_off_t)gli.Length.QuadPart;
	}

	size = _filelengthi64(fd);
	if (size < 0)
		errx(1, "Unable to determine size of fd %d", fd);
	return (hammer2_off_t)size;
}

/* ---- misc ----------------------------------------------------------------- */

int
sysctlbyname(const char *name, void *oldp, size_t *oldlenp,
	     const void *newp, size_t newlen)
{
	(void)name; (void)oldp; (void)oldlenp; (void)newp; (void)newlen;
	return -1;	/* not supported; callers fall back to defaults */
}

/* BSD strlcpy/strlcat (used by fsck_hammer2). */
size_t
strlcpy(char *dst, const char *src, size_t size)
{
	size_t srclen = strlen(src);
	if (size != 0) {
		size_t n = (srclen < size - 1) ? srclen : size - 1;
		memcpy(dst, src, n);
		dst[n] = '\0';
	}
	return srclen;
}

size_t
strlcat(char *dst, const char *src, size_t size)
{
	size_t dlen = strnlen(dst, size);
	size_t srclen = strlen(src);
	if (dlen == size)
		return size + srclen;
	if (srclen < size - dlen) {
		memcpy(dst + dlen, src, srclen + 1);
	} else {
		memcpy(dst + dlen, src, size - dlen - 1);
		dst[size - 1] = '\0';
	}
	return dlen + srclen;
}
