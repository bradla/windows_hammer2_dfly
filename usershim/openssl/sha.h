/*
 * usershim/openssl/sha.h - minimal OpenSSL SHA256 API shim.
 *
 * fsck_hammer2 uses OpenSSL's SHA256_Init/Update/Final to verify HAMMER2
 * SHA256-check-code blocks.  We provide the same surface backed by a
 * self-contained SHA256 (sha256_user.c) so no OpenSSL dependency is needed.
 */
#ifndef _H2U_OPENSSL_SHA_H_
#define _H2U_OPENSSL_SHA_H_

#include <stdint.h>
#include <stddef.h>

#define SHA256_DIGEST_LENGTH	32

typedef struct {
	uint32_t	state[8];
	uint64_t	bitcount;
	uint8_t		buffer[64];
	size_t		buflen;
} SHA256_CTX;

int SHA256_Init(SHA256_CTX *ctx);
int SHA256_Update(SHA256_CTX *ctx, const void *data, size_t len);
int SHA256_Final(unsigned char *md, SHA256_CTX *ctx);

#endif /* _H2U_OPENSSL_SHA_H_ */
