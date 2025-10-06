//////////////////////////////////////////////////////////////////////////
// IOPT HTTP Debug & Monitoring Server (Sockets Implementation)         //
// Author: Fernando J. G. Pereira  - 2015                               //
//////////////////////////////////////////////////////////////////////////
#ifndef ARDUINO
#ifdef HTTP_SERVER

#ifdef _WIN32
#  define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#  include <winsock2.h>
#  include <windows.h>
#  define SOCK_NONBLOCK 0
#  define MSG_NOSIGNAL  0
#  pragma comment( lib, "Ws2_32.lib" )
#else
#  define INVALID_SOCKET -1
#  define SOCKET_ERROR -1
#  define SOCKET int
#  include <unistd.h>
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <netinet/ip.h>
#  include <arpa/inet.h>
#endif

#include "http_server.h"
#include "net_types.h"


#define CONCAT(a,b,c)	  	a ## b ## c
#define GET_INPUT_INFO(m)       CONCAT( get_, m, _InputInfo )
#define GET_OUTPUT_INFO(m)      CONCAT( get_, m, _OutputInfo )
#define GET_MARKING_INFO(m)     CONCAT( get_, m, _MarkingInfo )
#define GET_TRANSITION_INFO(m)  CONCAT( get_, m, _TFiredInfo )
#define GET_MARKING_VAR(m)      CONCAT( get_, m, _NetMarking )
#define GET_PLACEOUT_VAR(m)     CONCAT( get_, m, _PlaceOutputSignals )
#define GET_EVTOUT_VAR(m)       CONCAT( get_, m, _EventOutputSignals )
#define FORCE_MARKING_VAR(m)    CONCAT( force_, m, _Marking )
#define FORCE_OUTPUTS_VAR(m)    CONCAT( force_, m, _Outputs )

#define GET_INPUTS	     	GET_INPUT_INFO(MODEL_NAME)
#define GET_OUTPUTS	     	GET_OUTPUT_INFO(MODEL_NAME)
#define GET_MARKING	     	GET_MARKING_INFO(MODEL_NAME)
#define GET_TRANSITIONS	     	GET_TRANSITION_INFO(MODEL_NAME)
#define FORCE_MARKING	     	FORCE_MARKING_VAR(MODEL_NAME)
#define FORCE_OUTPUTS	     	FORCE_OUTPUTS_VAR(MODEL_NAME)
#define GET_MARKING_PTR	     	GET_MARKING_VAR(MODEL_NAME)
#define GET_PLACEOUT_PTR     	GET_PLACEOUT_VAR(MODEL_NAME)
#define GET_EVTOUT_PTR     	GET_EVTOUT_VAR(MODEL_NAME)


extern iopt_param_info *input_fv, *output_fv;
extern void setup();


static SOCKET server_sock = -1;
static int data_fd = -1;
static int control_fd = -1;
static request_arg args[MAX_ARGS];
static const char* cmd = 0;
static const char* ans = 0;
static char buffer[BUFF_SIZE+1];
static iopt_param_info breakpoints[MODEL_N_TRANSITIONS+1];
static int step_cntr = 0;
static int prev_in[MODEL_N_INPUTS];
static int prev_out[MODEL_N_OUTPUTS];
static int prev_m[MODEL_N_PLACES];
static int prev_trace_control = -1;
static char debug = 0;
static const char* last_breakpoint = 0;
static char* password = PASSWORD;



void cmdGetAll( int fd, request_arg args[] );
void cmdGetInputs( int fd, request_arg args[] );
void cmdGetOutputs( int fd, request_arg args[] );
void cmdGetMarking( int fd, request_arg args[] );
void cmdForceInputs( int fd, request_arg args[] );
void cmdForceOutputs( int fd, request_arg args[] );
void cmdSetMarking( int fd, request_arg args[] );
void cmdSetOutputs( int fd, request_arg args[] );
void cmdGetFiredTr( int fd, request_arg args[] );
void cmdGetAllTr( int fd, request_arg args[] );
void cmdStart( int fd, request_arg args[] );
void cmdPause( int fd, request_arg args[] );
void cmdExecStep( int fd, request_arg args[] );
void cmdGetTraceMode( int fd, request_arg args[] );
void cmdSetBreakpoints( int fd, request_arg args[] );
void cmdGetBreakpoints( int fd, request_arg args[] );
void cmdGetModelName( int fd, request_arg args[] );
void cmdGetDataChannel( int fd, request_arg args[] );
void cmdGetDataStream( int fd, request_arg args[] );
void cmdReset( int fd, request_arg args[] );
static void parseRequest( int fd, char* request );
static int sendInfo( iopt_param_info* info, int append, int non_null );

static ioptnet_cmd all_cmds[] = {
    { "GetAll",		&cmdGetAll },
    { "GetInputs",	&cmdGetInputs },
    { "GetOutputs",	&cmdGetOutputs },
    { "GetMarking",	&cmdGetMarking },
    { "ForceInputs",	&cmdForceInputs },
    { "ForceOutputs",	&cmdForceOutputs },
    { "SetMarking",	&cmdSetMarking },
    { "SetOutputs",	&cmdSetOutputs },
    { "GetFiredTr",	&cmdGetFiredTr },
    { "GetAllTr",	&cmdGetAllTr },
    { "Start",		&cmdStart },
    { "Pause",		&cmdPause },
    { "ExecStep",	&cmdExecStep },
    { "GetTraceMode",	&cmdGetTraceMode },
    { "SetBreakpoints",	&cmdSetBreakpoints },
    { "GetBreakpoints",	&cmdGetBreakpoints },
    { "GetModelName",	&cmdGetModelName },
    { "GetDataChannel",	&cmdGetDataChannel },
    { "GetDataStream",	&cmdGetDataStream },
    { "Reset",		&cmdReset },
    { NULL, 0 },
};


static void closeConnection( int fd )
{
#ifdef _WIN32
    shutdown( fd, SD_BOTH );
    closesocket( fd );
#else
    shutdown( fd, SHUT_RDWR );
    close( fd );
#endif
}


static int sendAnswer( int fd, const char* msg )
{
    if( debug ) fprintf( stderr, "Send %s", msg );
    if( msg ) return send( fd, msg, strlen(msg), MSG_NOSIGNAL );
    else return 0;
}


static void sendError( int fd, const char* msg )
{
    sendAnswer( fd, msg );
    sendAnswer( fd, "Access-Control-Allow-Origin: *\n");
    closeConnection( fd );
}


void httpServer_init()
{
    int i, port = DEF_PORT;
    if( getenv( "IP_PORT" ) ) {
        port = atoi( getenv("IP_PORT") );
	fprintf( stderr, "IP Port = %d\n", port );
    }

    if( getenv("HTTP_DEBUG") ) debug = 1;
    if( getenv( "HTTP_PASSWORD" ) ) password = getenv( "HTTP_PASSWORD" );

#ifdef _WIN32
    WSADATA data;
    WSAStartup( 2, &data );
#endif

    if( server_sock >= 0 ) closeConnection( server_sock );

    if( data_fd >= 0 ) {
	closeConnection( data_fd );
	data_fd = -1;
    }

    server_sock = socket( AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0 );
    if( server_sock == INVALID_SOCKET ) {
        perror( "socket" );
	return;
    }

#ifdef SO_REUSEADDR
    int one = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&one, sizeof(int));
#endif

#ifdef IPTOS_LOWDELAY
    int tos = IPTOS_LOWDELAY;
    if( setsockopt( server_sock, IPPROTO_IP, IP_TOS,
		    (const char*)&tos, sizeof(tos) ) == SOCKET_ERROR ) {
        perror( "cannot set socket tos flag:" );
    }
#endif

#ifdef SO_PRIORITY
    int prio = 6;
    if( setsockopt( server_sock, SOL_SOCKET, SO_PRIORITY,
		    (const char*)&prio, sizeof(prio) ) == SOCKET_ERROR ) {
        perror( "cannot set socket priority flag:" );
    }
#endif

#ifdef _WIN32
    unsigned long mode = 1;
    ioctlsocket( server_sock, FIONBIO, &mode );
#endif

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if( bind( server_sock, (struct sockaddr*)&addr, sizeof(addr) ) ==
        SOCKET_ERROR ) {
        perror( "bind" );
	return;
    }

    if( listen( server_sock, MAX_CLIENTS ) == SOCKET_ERROR ) {
        perror( "listen" );
	return;
    }

    last_breakpoint = 0;
    for( i = 0; i < MODEL_N_TRANSITIONS; ++i ) breakpoints[i].value = 0;
}



void httpServer_getRequest()
{
    int i, n;
    struct sockaddr_in addr;
#ifdef _WIN32
    int len = sizeof( addr );
#else 
    socklen_t len = sizeof( addr );
#endif

    int fd = accept( server_sock, (struct sockaddr*)&addr, &len );
    if( fd == INVALID_SOCKET ) {
#ifdef _WIN32
	int err = WSAGetLastError();
	if( err != WSAEWOULDBLOCK && err != WSAEINTR )
	    fprintf( stderr, "Accept error: %d\n", err );
#else
	 if( errno != EAGAIN && errno != EWOULDBLOCK ) perror( "accept" );
#endif
	return;
    }

#ifdef IPTOS_LOWDELAY
    int tos = IPTOS_LOWDELAY;
    if( setsockopt( fd, IPPROTO_IP, IP_TOS,
		    (const char*)&tos, sizeof(tos) ) == SOCKET_ERROR ) {
        perror( "cannot set socket tos flag:" );
    }
#endif

#ifdef SO_PRIORITY
    int prio = 6;
    if( setsockopt( fd, SOL_SOCKET, SO_PRIORITY,
		    (const char*)&prio, sizeof(prio) ) == SOCKET_ERROR ) {
        perror( "cannot set socket priority flag:" );
	return;
    }
#endif

#ifdef _WIN32
    unsigned long mode = 0;
    ioctlsocket( fd, FIONBIO, &mode );
#endif
   
    len = 0;
    n = 0;
    char* ptr = buffer;
    char eoln = 0;
    do {
	n = recv( fd, ptr, BUFF_SIZE-len, 0 );
	if( n == SOCKET_ERROR ) {
#ifdef _WIN32
	    int err = WSAGetLastError();
	    fprintf( stderr, "Recv error: %d\n", err );
#else
	    perror( "recv" );
#endif
	    return;
	}
	len += n;
	for( i = 0; i < n; ++i ) {
	    if( ptr[i] == '\n' || ptr[i] == '\r' ) {
	        ptr[i] = '\0';
	        eoln = 1;
	        break;
	    }
	}
	ptr += n;
    }
    while( len < BUFF_SIZE && !eoln );
    buffer[len] = '\0';

    parseRequest( fd, buffer );
}



static const char* getArg( const char* key, request_arg args[] )
{
    int i;
    for( i = 0; args[i].name; ++i ) {
        if( strcmp( key, args[i].name ) == 0 && args[i].value )
	    return args[i].value;
    }
    return 0;
}


static void parseRequest( int fd, char* request )
{
    int i;
    const char* token = 0;

    if( strncasecmp( request, "GET ", 4 ) ) {
        sendError( fd, "HTTP/1.0 400 Bad Request\n" );
	return;
    }

    if( debug ) fprintf( stderr, "\nREQ = '%s'\n", request );

    for( i = 4; request[i] != '\0'; ++i ) {
        if( request[i] == '/' ) {
	    if( request[i+1] == '/' ) { ++i; continue; }
	    token = strtok( request+(i+1), " ?" );
	    break;
	}
    }

    if( token == 0 ) {
        sendError( fd, "HTTP/1.0 400 Bad Request\n" );
	return;
    }
    else cmd = token;

    int n_args = 0;
    while( n_args < MAX_ARGS && (token = strtok( NULL, "= " )) ) {
        args[n_args].name = token;
        token = strtok( NULL, "& " );
        args[n_args].value = token;
	++n_args;
    }
    args[n_args].name = 0;
    args[n_args].value = 0;

    const char* pw = getArg( "pw", args );
    if( pw == 0 || strcmp( pw, password ) ) {
        sendError( fd, "HTTP/1.0 401 Unauthorized\n" );
	return;
    }

    for( i = 0; all_cmds[i].cmd_name; ++i ) {
        if( strcmp( cmd, all_cmds[i].cmd_name ) == 0 ) {
	    control_fd = fd;
	    (*all_cmds[i].func)( fd, args );
	    return;
	}
    }

    sendError( fd, "HTTP/1.0 400 Bad Request\n" );
}



static int sendDiff( int n, iopt_param_info info[], int prev[] )
{
    char aux[256];
    int i, j = 0, changes = 0;
    for( i = 0, j = 0; i < n; ++i ) {
        if( info[i].value != prev[i] ) {
	    prev[i] = info[i].value;
	    changes = 1;
	    if( j>0 ) sprintf( aux, ",\"%s\":%d", info[i].name, info[i].value );
	    else sprintf( aux, "\"%s\":%d", info[i].name, info[i].value );
	    ++j;
	    strcat( buffer, aux );
	}
    }
    return changes;
}



static void sendDelta()
{
    int changes = (step_cntr >= 10000);

    if( step_cntr > 1 )
	sprintf( buffer, "data:{\"steps\":%d,\"in\":{", step_cntr );
    else
	sprintf( buffer, "data:{\"in\":{" );

    changes |= sendDiff( MODEL_N_INPUTS, GET_INPUTS(), prev_in );

    strcat( buffer, "},\"out\":{" );
    changes |= sendDiff( MODEL_N_OUTPUTS, GET_OUTPUTS(), prev_out );

    strcat( buffer, "},\"m\":{" );
    changes |= sendDiff( MODEL_N_PLACES, GET_MARKING(), prev_m );

    strcat( buffer, "},\"tf\":" );
    int tr = sendInfo( GET_TRANSITIONS(), 1, 1 );
    if( trace_control != TRACE_PAUSE ||
        trace_control != prev_trace_control ) changes |= tr;

    if( trace_control != prev_trace_control ) {
        prev_trace_control = trace_control;
	if( trace_control < 0 ) strcat( buffer, ",\"TraceMode\":\"Running\"" );
	else if( trace_control==0 ) strcat(buffer, ",\"TraceMode\":\"Paused\"");
	else strcat( buffer, ",\"TraceMode\":\"StepByStep\"" );
	changes = 1;
    }

    if( last_breakpoint ) {
	strcat( buffer, ",\"Breakpoint\":\"" );
	strcat( buffer, last_breakpoint );
	strcat( buffer, "\"" );
	last_breakpoint = 0;
	changes = 1;
    }

    if( changes ) {
	strcat( buffer, "}\n\n" );
	if( sendAnswer( data_fd, buffer ) < 0 ) {
	    closeConnection( data_fd );
	    data_fd = -1;
	}
	step_cntr = 0;
    }
}



void httpServer_sendResponse()
{
    if( control_fd >= 0 ) {
	sendAnswer( control_fd,
	            "HTTP/1.0 200 OK\n"
		    "Content-type: application/json; charset=utf-8\n"
	            "Access-Control-Allow-Origin: *\n"
	            "Pragma: no-cache\n\n"
	);

	if( ans == 0 ) cmdGetAll( control_fd, 0 );
	sendAnswer( control_fd, ans );
	ans = 0;
    }

    if( data_fd >= 0 ) {
	sendDelta();
	++step_cntr;
    }
}



void httpServer_disconnectClient()
{
    if( control_fd >= 0 ) closeConnection( control_fd );
    control_fd = -1;
}



void httpServer_finish()
{
    closeConnection( server_sock );
}



void httpServer_checkBreakPoints()
{
    int i;
    if( trace_control == TRACE_PAUSE ) return;
    iopt_param_info *tr = GET_TRANSITIONS();
    for( i = 0; i < MODEL_N_TRANSITIONS; ++i ) {
        if( tr[i].value == 1 && breakpoints[i].value == 1 ) {
	    trace_control = TRACE_PAUSE;
	    if( data_fd ) last_breakpoint = tr[i].name;
	    break;
	}
    }
}


static int sendInfo( iopt_param_info* info, int append, int non_null )
{
    int i, j = 0;
    char aux[256];
    if( append ) strcat( buffer, "{" );
    else strcpy( buffer, "{" );

    for( i = 0; info[i].name != 0; ++i ) {
        if( non_null && info[i].value == 0 ) continue;
        if( j++ > 0 ) sprintf( aux, ",\"%s\":%d", info[i].name, info[i].value );
        else sprintf( aux, "\"%s\":%d", info[i].name, info[i].value );
	strcat( buffer, aux );
    }
    strcat( buffer, "}" );
    ans = buffer;
    return (j != 0);
}


void cmdGetAll( int fd, request_arg args[] )
{
    strcpy( buffer, "{\"in\":" );
    sendInfo( GET_INPUTS(), 1, 0 );
    strcat( buffer, ",\"out\":" );
    sendInfo( GET_OUTPUTS(), 1, 0 );
    strcat( buffer, ",\"m\":" );
    sendInfo( GET_MARKING(), 1, 0 );
    strcat( buffer, "}\n" );
}


void cmdGetInputs( int fd, request_arg args[] )
{
    sendInfo( GET_INPUTS(), 0, 0 );
}


void cmdGetOutputs( int fd, request_arg args[] )
{
    sendInfo( GET_OUTPUTS(), 0, 0 );
}


void cmdGetMarking( int fd, request_arg args[] )
{
    sendInfo( GET_MARKING(), 0, 0 );
}


static int forceIO( request_arg args[],
                    iopt_param_info fv[], iopt_param_info v[] )
{
    int i, nf = 0;
    const char* a;

    for( i = 0; v[i].name; ++i ) {
        a = getArg( v[i].name, args );
	if( a ) {
	    fv[nf].name = v[i].name;
	    fv[nf].value = atoi(a);
	    ++nf;
	}
    }
    fv[nf++].name = NULL;
    return nf;
}


void cmdForceInputs( int fd, request_arg args[] )
{
    static iopt_param_info fv[MODEL_N_INPUTS+1];

    if( forceIO( args, fv, GET_INPUTS() ) > 0 ) input_fv = fv;
    else input_fv = 0;
    ans="{\"result\":\"OK\"}\n";
}



void cmdForceOutputs( int fd, request_arg args[] )
{
    static iopt_param_info fv[MODEL_N_OUTPUTS+1];

    if( forceIO( args, fv, GET_OUTPUTS() ) > 0 ) output_fv = fv;
    else output_fv = 0;
    ans="{\"result\":\"OK\"}\n";
}


void cmdSetMarking( int fd, request_arg args[] )
{
    int i;
    iopt_param_info *info, fv[MODEL_N_PLACES+1];

    if( forceIO( args, fv, GET_MARKING() ) > 0 ) 
        FORCE_MARKING( fv, GET_MARKING_PTR() );

    info = GET_MARKING();
    for( i = 0; i < MODEL_N_PLACES; ++i ) prev_m[i] = info[i].value;
    sendInfo( info, 0, 0 );
}


void cmdSetOutputs( int fd, request_arg args[] )
{
    int i;
    iopt_param_info *info, fv[MODEL_N_OUTPUTS+1];

    if( forceIO( args, fv, GET_OUTPUTS() ) > 0 ) 
        FORCE_OUTPUTS( fv, GET_PLACEOUT_PTR(), GET_EVTOUT_PTR() );

    info = GET_OUTPUTS();
    for( i = 0; i < MODEL_N_OUTPUTS; ++i ) prev_out[i] = info[i].value;
    sendInfo( info, 0, 0 );
}


void cmdGetFiredTr( int fd, request_arg args[] )
{
    sendInfo( GET_TRANSITIONS(), 0, 1 );
}


void cmdGetAllTr( int fd, request_arg args[] )
{
    sendInfo( GET_TRANSITIONS(), 0, 0 );
}


void cmdStart( int fd, request_arg args[] )
{
    trace_control = TRACE_CONT_RUN;
    ans="{\"TraceMode\":\"Running\"}\n";
}


void cmdPause( int fd, request_arg args[] )
{
    trace_control = TRACE_PAUSE;
    ans="{\"TraceMode\":\"Paused\"}\n";
}


void cmdGetTraceMode( int fd, request_arg args[] )
{
    if( trace_control == TRACE_CONT_RUN ) ans="{\"TraceMode\":\"Running\"}\n";
    else if( trace_control == TRACE_PAUSE ) ans="{\"TraceMode\":\"Paused\"}\n";
    else ans="{\"TraceMode\":\"StepByStep\"}\n";
}


void cmdExecStep( int fd, request_arg args[] )
{
    const char* n = getArg( "n", args );
    if( n ) {
        trace_control = TRACE_N_STEPS( atoi(n) );
	if( trace_control < 0 ) trace_control = 0;
    }
    else trace_control = TRACE_SINGLE_STEP;
    ans = 0;
}


void cmdSetBreakpoints( int fd, request_arg args[] )
{
    int i;
    const char* v;

    iopt_param_info *tr = GET_TRANSITIONS();

    for( i = 0; i < MODEL_N_TRANSITIONS; ++i ) {
        v = getArg( tr[i].name, args );
	breakpoints[i].name = tr[i].name;
	breakpoints[i].value = (v != 0) ? atoi(v) : 0;
    }
    breakpoints[i].name = NULL;
    ans="{\"result\":\"OK\"}\n";
}


void cmdGetBreakpoints( int fd, request_arg args[] )
{
    sendInfo( breakpoints, 0, 1 );
}


void cmdGetModelName( int fd, request_arg args[] )
{
    ans = "{\"model\":\"" MODEL_NAME_STR "\",\"version\":\"" MODEL_VERSION "\"}\n";
}


void cmdGetDataChannel( int fd, request_arg args[] )
{
    ans = "{\"url\":\"/GetDataStream\"}\n";
}


void cmdGetDataStream( int fd, request_arg args[] )
{
    if( data_fd >= 0 ) {
        //sendError( fd, "HTTP/1.0 503 Service Unavailable\n" );
	//return;
        sendAnswer( data_fd, "data: Disconnect\n" );
	closeConnection( data_fd );
	data_fd = -1;
    }

    data_fd = fd;
    control_fd = -1;
    last_breakpoint = 0;

#ifdef IPTOS_THROUGHPUT
    int tos = IPTOS_LOWDELAY | IPTOS_THROUGHPUT;
    if( setsockopt( server_sock, IPPROTO_IP, IP_TOS,
		    (const char*)&tos, sizeof(tos) ) < 0 ) {
        perror( "cannot set socket tos flag:" );
    }
#endif 

    sendAnswer( fd, "HTTP/1.0 200 OK\n"
                    "Content-type: text/event-stream\n"
                    "Access-Control-Allow-Origin: *\n"
                    "Pragma: no-cache\n\n");
    sendAnswer( fd, "retry: 5000\n"
                    "data:");
    cmdGetAll( fd, 0 );
    sendAnswer( fd, buffer );
    sendAnswer( fd, "\n");
    step_cntr = 0;
   
    int i;
    iopt_param_info* info = GET_INPUTS();
    for( i = 0; i < MODEL_N_INPUTS; ++i ) prev_in[i] = info[i].value;
    info = GET_OUTPUTS();
    for( i = 0; i < MODEL_N_OUTPUTS; ++i ) prev_out[i] = info[i].value;
    info = GET_MARKING();
    for( i = 0; i < MODEL_N_PLACES; ++i ) prev_m[i] = info[i].value;
    prev_trace_control = trace_control;
}


void cmdReset( int fd, request_arg args[] )
{
    ans = "{\"result\":\"OK\"}\n";
    httpServer_sendResponse();
    closeConnection( fd );
    control_fd = -1;
    setup();
}

#endif
#endif
