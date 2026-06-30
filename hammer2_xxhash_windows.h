/*
 * hammer2_xxhash_windows.h - self-contained XXH64 for the HAMMER2 Windows port.
 *
 * HAMMER2 uses XXH64 as one of its block check-code algorithms.  The Linux port
 * borrowed the kernel's xxh64(); on Windows we provide a standalone, spec-exact
 * implementation (xxHash by Yann Collet, BSD-2-Clause) so on-disk check codes
 * validate identically.
 */

#ifndef _HAMMER2_XXHASH_WINDOWS_H_
#define _HAMMER2_XXHASH_WINDOWS_H_

/* Only needs fixed-width ints + memcpy; avoid the kernel shim so this header is
 * usable by the userland tools (newfs/fsck) too. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define XXH_PRIME64_1 0x9E3779B185EBCA87ULL
#define XXH_PRIME64_2 0xC2B2AE3D27D4EB4FULL
#define XXH_PRIME64_3 0x165667B19E3779F9ULL
#define XXH_PRIME64_4 0x85EBCA77C2B2AE63ULL
#define XXH_PRIME64_5 0x27D4EB2F165667C5ULL

static inline uint64_t xxh_rotl64(uint64_t x, int r)
{
	return (x << r) | (x >> (64 - r));
}

static inline uint64_t xxh_read64(const void *p)
{
	uint64_t v;
	memcpy(&v, p, sizeof(v));	/* host endian, matches the kernel */
	return v;
}

static inline uint32_t xxh_read32(const void *p)
{
	uint32_t v;
	memcpy(&v, p, sizeof(v));
	return v;
}

static inline uint64_t xxh64_round(uint64_t acc, uint64_t input)
{
	acc += input * XXH_PRIME64_2;
	acc = xxh_rotl64(acc, 31);
	acc *= XXH_PRIME64_1;
	return acc;
}

static inline uint64_t xxh64_merge_round(uint64_t acc, uint64_t val)
{
	val = xxh64_round(0, val);
	acc ^= val;
	acc = acc * XXH_PRIME64_1 + XXH_PRIME64_4;
	return acc;
}

static inline uint64_t xxh64(const void *input, size_t len, uint64_t seed)
{
	const uint8_t *p = (const uint8_t *)input;
	const uint8_t *const end = p + len;
	uint64_t h64;

	if (len >= 32) {
		const uint8_t *const limit = end - 32;
		uint64_t v1 = seed + XXH_PRIME64_1 + XXH_PRIME64_2;
		uint64_t v2 = seed + XXH_PRIME64_2;
		uint64_t v3 = seed + 0;
		uint64_t v4 = seed - XXH_PRIME64_1;

		do {
			v1 = xxh64_round(v1, xxh_read64(p)); p += 8;
			v2 = xxh64_round(v2, xxh_read64(p)); p += 8;
			v3 = xxh64_round(v3, xxh_read64(p)); p += 8;
			v4 = xxh64_round(v4, xxh_read64(p)); p += 8;
		} while (p <= limit);

		h64 = xxh_rotl64(v1, 1) + xxh_rotl64(v2, 7) +
		      xxh_rotl64(v3, 12) + xxh_rotl64(v4, 18);
		h64 = xxh64_merge_round(h64, v1);
		h64 = xxh64_merge_round(h64, v2);
		h64 = xxh64_merge_round(h64, v3);
		h64 = xxh64_merge_round(h64, v4);
	} else {
		h64 = seed + XXH_PRIME64_5;
	}

	h64 += (uint64_t)len;

	while (p + 8 <= end) {
		uint64_t k1 = xxh64_round(0, xxh_read64(p));
		h64 ^= k1;
		h64 = xxh_rotl64(h64, 27) * XXH_PRIME64_1 + XXH_PRIME64_4;
		p += 8;
	}
	if (p + 4 <= end) {
		h64 ^= (uint64_t)xxh_read32(p) * XXH_PRIME64_1;
		h64 = xxh_rotl64(h64, 23) * XXH_PRIME64_2 + XXH_PRIME64_3;
		p += 4;
	}
	while (p < end) {
		h64 ^= (uint64_t)(*p) * XXH_PRIME64_5;
		h64 = xxh_rotl64(h64, 11) * XXH_PRIME64_1;
		p++;
	}

	h64 ^= h64 >> 33;
	h64 *= XXH_PRIME64_2;
	h64 ^= h64 >> 29;
	h64 *= XXH_PRIME64_3;
	h64 ^= h64 >> 32;
	return h64;
}

#endif /* _HAMMER2_XXHASH_WINDOWS_H_ */
