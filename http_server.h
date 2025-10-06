#ifdef ARDUINO
#  include "net_server.h"
#else

#ifndef __IOPTNET_DEFS
#define __IOPTNET_DEFS

#define DEF_PORT		8000
#define MAX_CLIENTS		10
#define MAX_ARGS		100
#define BUFF_SIZE		4096
#define PASSWORD		"1234"


typedef struct {
    const char* name;
    const char* value;
} request_arg;


typedef void (*cmd_func)( int fd, request_arg args[] );

typedef struct {
    const char *cmd_name;
    cmd_func func;
} ioptnet_cmd;


extern void httpServer_init();
extern void httpServer_getRequest();
extern void httpServer_sendResponse();
extern void httpServer_disconnectClient();
extern void httpServer_finish();
extern void httpServer_checkBreakPoints();


#endif
#endif 
