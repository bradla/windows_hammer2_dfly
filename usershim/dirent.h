#ifndef _H2U_DIRENT_H_
#define _H2U_DIRENT_H_
struct dirent { char d_name[260]; };
typedef struct H2U_DIR H2U_DIR;
#define DIR H2U_DIR
#endif
