/*
 * net_server.h
 *
 * Created: 21/01/2015 22:42:46
 *  Author: arildo
 */ 

#ifdef ARDUINO

#define HTTP_SERVER

#ifndef NET_SERVER_H_
#define NET_SERVER_H_

#define DBG_INFO
#define ARDUINO_IP_ADDR 192,168,1,177

#include "net_types.h"
#include "Ethernet.h"
#include "SPI.h"
#include <avr/wdt.h>
	
#define CONCAT_MODEL_NAME_3( a, b, c )           a ## _ ## b ## c
#define CONCAT_MODEL_NAME_4( a, b, c, d )        a ## b ## _ ## c ## d
#define CONCAT_MODEL_NAME_3A( a, b, c)        a ## b ## c

#define DO_GET_INFO( model,type )     CONCAT_MODEL_NAME_3A(get_,model, type)
#define GET_INPUT_INFO                DO_GET_INFO(MODEL_NAME,_InputInfo)
#define GET_OUTPUT_INFO               DO_GET_INFO(MODEL_NAME,_OutputInfo)
#define GET_TFIRED_INFO               DO_GET_INFO(MODEL_NAME,_TFiredInfo)
#define GET_MARKING_INFO              DO_GET_INFO(MODEL_NAME,_MarkingInfo)

#define TYPE_INPUTSINFO_STRUCT(model,type)    CONCAT_MODEL_NAME_3( model, type,_info )
#define INFO_STRUCT(type)                TYPE_INPUTSINFO_STRUCT(MODEL_NAME,type)

#define DO_FORCE_VALUES( model, type )      CONCAT_MODEL_NAME_3A( force_ , model , type )
#define FORCE_INPUTS_VALUES(type)            DO_FORCE_VALUES(MODEL_NAME, _Inputs)
#define FORCE_OUTPUTS_VALUES                 DO_FORCE_VALUES(MODEL_NAME, _Outputs)
#define FORCE_MARKING_VALUES                DO_FORCE_VALUES(MODEL_NAME, _Marking)

#define GET_SIGNALS(m, type)     CONCAT_MODEL_NAME_3A( get_, m,  type)
#define GET_PLACEOUT_PTR     	GET_SIGNALS(MODEL_NAME, _PlaceOutputSignals)
#define GET_EVTOUT_PTR     	GET_SIGNALS(MODEL_NAME, _EventOutputSignals)
#define GET_MARKING_PTR	     	GET_SIGNALS(MODEL_NAME, _NetMarking)

#define GETTRANSITIONSFIRED "getTansFired";
#define  PASSWORD           "1234" 

#define CMD_GET_ALL         "GetAll"
#define CMD_GET_INPUTS      "GetInputs"
#define CMD_GET_OUTPUTS     "GetOutputs"
#define CMD_GET_MARKING     "GetMarking"
#define CMD_GET_MODELNAME   "GetModelName"
#define CMD_GET_TRANSFIRED  "GetFiredTr"
#define CMD_GET_TRANSITIONS "GetAllTr"
#define CMD_GET_BREAKPOINT  "GetBreakpoints"
#define CMD_GET_TRACEMODE   "GetTraceMode"
#define CMD_GET_FEEDURL     "GetDataChannel"

#define CMD_SET_INPUT       "ForceInputs"
#define CMD_SET_OUPUT       "ForceOutput"
#define CMD_SET_BREAKPOINT  "SetBreakpoints"
#define CMD_SET_START       "Start"
#define CMD_SET_PAUSE       "Pause"
#define CMD_SET_EXECSTEP    "ExecStep"
#define CMD_SET_MARKING_ONCE "SetMarking"
#define CMD_SET_OUPUT_ONCE   "SetOutputs"

#define CMD_RESET           "Reset"

#define AUTH_FAIL           "E2"

#define STEPACK             10000
#define SYNCPORT            80
#define FEEDPORT            81

#define DS_INPUTS          "in"
#define DS_OUTPUTS         "out"
#define DS_MARKINGS        "m"
#define DS_TRANSITIONS     "tf"
#define DS_BREAKPOINTS     "bp"
#define DS_STEPS           "n"
#define DS_TRACECONTROL    "tm"
#define DS_ACTIVEBREAKPOINT "Breakpoint"



void httpServer_getRequest();
void httpServer_sendResponse();
void httpServer_init();
void httpServer_disconnectClient();
void httpServer_checkBreakPoints();


void setInput(String buffer);
void setOutput(String buffer);
String getParameter(String pName, String pList);
void getResponse(Client &c, iopt_param_info* attrList, int filterValue);
void getMarkingResponse(Client &c);
void getOutputResponse(Client &c);
void getInputResponse(Client &c);
void getAll(Client &c);
void ParseReceivedRequest(String &buffer, Client &c);
void getTFiredResponse(Client &c);
void setBreakPoint(String buffer);
void getBreakPointResponse(Client &c);
void getTransitionsResponse(Client &c);
void resetBoard();
void sendFeedResponse(Client &c);
void getChangedSignals(Client &c, int* lastState, iopt_param_info* newState, int len, String ds, boolean &first_struct);
void sendTraceControl(Client &c, boolean &first_struct);
void sendActiveBreakPoint(Client &c, boolean &first_struct);
String getTraceMode(int trace);
static int forceIO(String b,iopt_param_info fv[], iopt_param_info v[] );
void setOutputs_once(String buffer);
void setMarking_once(String buffer);




#endif /* NET_SERVER_H_ */
#endif /* ARDUINO */
