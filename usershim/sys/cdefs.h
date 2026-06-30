/* usershim/sys/cdefs.h - minimal BSD <sys/cdefs.h> for <sys/queue.h>/<sys/tree.h> */
#ifndef _H2U_SYS_CDEFS_H_
#define _H2U_SYS_CDEFS_H_
#ifndef __BEGIN_DECLS
#define __BEGIN_DECLS
#define __END_DECLS
#endif
#ifndef __unused
#define __unused
#endif
#ifndef __inline
#define __inline __inline
#endif
#ifndef __restrict
#define __restrict
#endif
#ifndef __containerof
#define __containerof(ptr, type, member) \
	((type *)((char *)(ptr) - offsetof(type, member)))
#endif
#endif
