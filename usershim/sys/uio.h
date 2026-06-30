#ifndef _H2U_UIO_H_
#define _H2U_UIO_H_
#include <stddef.h>
#ifndef _H2U_HAS_IOVEC
#define _H2U_HAS_IOVEC
struct iovec { void *iov_base; size_t iov_len; };
#endif
#endif
