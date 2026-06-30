#ifndef _H2U_SYS_TIME_H_
#define _H2U_SYS_TIME_H_
#include <time.h>
#include "hammer2_user_windows.h"
struct timeval { long tv_sec; long tv_usec; };
static __inline int gettimeofday(struct timeval *tv, void *tz)
{
	FILETIME ft; ULARGE_INTEGER u; unsigned long long t;
	(void)tz;
	GetSystemTimePreciseAsFileTime(&ft);
	u.LowPart = ft.dwLowDateTime; u.HighPart = ft.dwHighDateTime;
	t = u.QuadPart - 116444736000000000ULL;	/* 1601->1970 */
	tv->tv_sec = (long)(t / 10000000ULL);
	tv->tv_usec = (long)((t % 10000000ULL) / 10);
	return 0;
}
#endif
