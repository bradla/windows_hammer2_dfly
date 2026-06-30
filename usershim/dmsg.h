/* usershim/dmsg.h - opaque DMSG types so the hammer2 CLI header parses.
 * The offline commands (show/freemap/volume-list) don't drive DMSG. */
#ifndef _H2U_DMSG_H_
#define _H2U_DMSG_H_
typedef struct dmsg_msg dmsg_msg_t;
typedef struct dmsg_state dmsg_state_t;
typedef struct dmsg_iocom dmsg_iocom_t;
typedef struct dmsg_master_service_info dmsg_master_service_info_t;
#endif
