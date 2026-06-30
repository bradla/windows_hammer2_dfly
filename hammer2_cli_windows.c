/*
 * hammer2_cli_windows.c - Windows entry point for the OFFLINE hammer2(8)
 * subcommands.  These read the device directly via ondisk.c (no mount, no
 * ioctl) and so port cleanly:
 *
 *   hammer2 volume-list <device>   - list the volumes of a HAMMER2 fs
 *   hammer2 pfs-list <device>      - list the PFSes (super-root scan)
 *
 * The networked / ioctl commands (show/freemap's DMSG shell, etc.) are not
 * built here.  For an offline blockref dump use `fsck_hammer2 -v`.
 */

#ifndef HAMMER2_USERLAND
#define HAMMER2_USERLAND		/* normally set by the build; harmless here */
#endif
#include "hammer2_user_windows.h"
#include <vfs/hammer2/hammer2_disk.h>
#include "hammer2_subs.h"

/* ondisk.c (upstream) + the Windows accessor added for the offline tools. */
extern void hammer2_init_volumes(const char *blkdevs, int rdonly);
extern void hammer2_print_volumes(const hammer2_ondisk_t *fsp);
extern const hammer2_ondisk_t *hammer2_get_ondisk(void);

/* ---- volume-list ---------------------------------------------------------- */

static int
cmd_volume_list(const char *device)
{
	hammer2_init_volumes(device, 1);
	hammer2_print_volumes(hammer2_get_ondisk());
	return 0;
}

/* ---- pfs-list (offline super-root scan, mirrors fsck's read_media path) ---- */

/*
 * Read the media referenced by `bref` into `media`.  Returns the data size in
 * *bytes, or 0 on error.  Equivalent to fsck_hammer2's read_media().
 */
static size_t
read_media(const hammer2_blockref_t *bref, hammer2_media_data_t *media)
{
	hammer2_off_t io_off, io_base;
	size_t bytes, io_bytes, boff;
	int fd;

	bytes = (bref->data_off & HAMMER2_OFF_MASK_RADIX);
	if (bytes)
		bytes = (size_t)1 << bytes;
	if (!bytes)
		return 0;

	io_off = bref->data_off & ~HAMMER2_OFF_MASK_RADIX;
	io_base = io_off & ~(hammer2_off_t)(HAMMER2_LBUFSIZE - 1);
	boff = (size_t)(io_off - io_base);

	io_bytes = HAMMER2_LBUFSIZE;
	while (io_bytes + boff < bytes)
		io_bytes <<= 1;
	if (io_bytes > sizeof(*media))
		return 0;

	fd = hammer2_get_volume_fd(io_off);
	if (lseek(fd, io_base - hammer2_get_volume_offset(io_base), SEEK_SET) < 0)
		return 0;
	if (read(fd, media, (unsigned)io_bytes) != (ssize_t)io_bytes)
		return 0;
	if (boff)
		memmove(media, (char *)media + boff, bytes);
	return bytes;
}

static void
print_pfs(const hammer2_inode_data_t *ipdata)
{
	const hammer2_inode_meta_t *meta = &ipdata->meta;
	char name[HAMMER2_INODE_MAXNAME + 1];
	char *clid = NULL;
	const char *type_str;

	if (meta->pfs_type == HAMMER2_PFSTYPE_MASTER) {
		type_str = (meta->pfs_subtype == HAMMER2_PFSSUBTYPE_NONE)
		    ? "MASTER" : hammer2_pfssubtype_to_str(meta->pfs_subtype);
	} else {
		type_str = hammer2_pfstype_to_str(meta->pfs_type);
	}

	memcpy(name, ipdata->filename, sizeof(ipdata->filename));
	name[meta->name_len < sizeof(name) ? meta->name_len : sizeof(name) - 1] = 0;

	hammer2_uuid_to_str(&meta->pfs_clid, &clid);
	printf("%-11s %s %s\n", type_str, clid ? clid : "?", name);
	free(clid);
}

/* Scan a blockref: a PFS inode prints; the super-root inode + indirect blocks
 * recurse into their child blockrefs. */
static void
scan_bref(const hammer2_blockref_t *bref)
{
	static hammer2_media_data_t media;	/* 64K; keep off the stack */
	hammer2_media_data_t local;
	size_t bytes, i, n;

	switch (bref->type) {
	case HAMMER2_BREF_TYPE_INODE:
		if (!read_media(bref, &local))
			return;
		if (local.ipdata.meta.pfs_type == HAMMER2_PFSTYPE_SUPROOT) {
			/* super-root: its blockset references the PFS inodes */
			for (i = 0; i < HAMMER2_SET_COUNT; ++i)
				scan_bref(&local.ipdata.u.blockset.blockref[i]);
		} else {
			print_pfs(&local.ipdata);
		}
		break;
	case HAMMER2_BREF_TYPE_INDIRECT:
		bytes = read_media(bref, &media);
		n = bytes / sizeof(hammer2_blockref_t);
		for (i = 0; i < n; ++i)
			scan_bref(&media.npdata[i]);
		break;
	default:
		break;
	}
}

static int
cmd_pfs_list(const char *device)
{
	hammer2_volume_data_t *voldata;
	int i;

	hammer2_init_volumes(device, 1);
	voldata = hammer2_read_root_volume_header();
	if (voldata == NULL) {
		fprintf(stderr, "hammer2: cannot read volume header\n");
		return 1;
	}

	printf("Type        "
	       "ClusterId (pfs_clid)                 "
	       "Label on %s\n", device);
	for (i = 0; i < HAMMER2_SET_COUNT; ++i)
		scan_bref(&voldata->sroot_blockset.blockref[i]);

	free(voldata);
	return 0;
}

/* ---- dispatch ------------------------------------------------------------- */

/*
 * These tools read the raw HAMMER2 volume offline, so they need the backing
 * image file or \\.\PhysicalDriveN -- NOT a mounted drive letter (a Dokan mount
 * exposes the filesystem view, not the on-disk structure).  Catch the common
 * "X:" / "X:\" mistake with a helpful message.
 */
static int
is_drive_letter(const char *p)
{
	if (p[0] && p[1] == ':' && (p[2] == '\0' ||
	    ((p[2] == '\\' || p[2] == '/') && p[3] == '\0')))
		return 1;
	return 0;
}

static void
usage(const char *prog)
{
	fprintf(stderr,
	    "usage: %s volume-list <device>   list the volumes of a HAMMER2 fs\n"
	    "       %s pfs-list <device>      list the PFSes\n"
	    "\n"
	    "note: show/freemap need the DMSG+OpenSSL layer (not ported); use\n"
	    "      `fsck_hammer2 -v <device>` for an offline blockref dump.\n",
	    prog, prog);
}

int
main(int ac, char **av)
{
	const char *prog = av[0];

	if (ac < 3) {
		usage(prog);
		return 1;
	}

	if (strcmp(av[1], "volume-list") == 0 || strcmp(av[1], "pfs-list") == 0) {
		if (is_drive_letter(av[2])) {
			fprintf(stderr,
			    "hammer2: '%s' is a mounted drive letter; these "
			    "commands read the\n"
			    "         on-disk volume offline -- pass the backing "
			    "image file or\n"
			    "         \\\\.\\PhysicalDriveN you mounted, not the "
			    "drive letter.\n", av[2]);
			return 1;
		}
		if (strcmp(av[1], "volume-list") == 0)
			return cmd_volume_list(av[2]);
		return cmd_pfs_list(av[2]);
	}

	usage(prog);
	return 1;
}
