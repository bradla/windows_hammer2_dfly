/* usershim/sys/ioctl.h - terminal-size ioctl shim (always fails -> default width) */
#ifndef _H2U_SYS_IOCTL_H_
#define _H2U_SYS_IOCTL_H_
struct winsize { unsigned short ws_row, ws_col, ws_xpixel, ws_ypixel; };
#ifndef TIOCGWINSZ
#define TIOCGWINSZ 0x5413
#endif
static int ioctl(int fd, unsigned long req, ...) { (void)fd; (void)req; return -1; }
#endif
