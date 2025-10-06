/*
 * net_server.c
 *
 * Created: 22/01/2015 23:37:51
 *  Author: arildo
 */ 

#ifdef ARDUINO

#include "net_server.h"
#include "net_types.h"


extern int trace_control;
extern iopt_param_info *input_fv;
extern iopt_param_info *output_fv;
extern iopt_param_info INFO_STRUCT(input)[];
extern iopt_param_info INFO_STRUCT(output)[];
extern iopt_param_info INFO_STRUCT(tfired)[];
//iopt_param_info *in_portMap;
//iopt_param_info *out_portMap;

#define NO_PARM_FOUND "NULL"

String cmd="NULL";
//char aux_buffer[50];
String buffer;
EthernetClient cl;
EthernetClient feed_cl;
iopt_param_info breakPoints[MODEL_N_TRANSITIONS + 1];
int lastInputState[MODEL_N_INPUTS];
int lastOutputState[MODEL_N_OUTPUTS];
int lastMarkingState[MODEL_N_PLACES];
int lastTransitionState[MODEL_N_TRANSITIONS];
int lastTraceControl = TRACE_CONT_RUN;
//iopt_param_info lastTrasitionState[MODEL_N_TRANSITIONS];
boolean first_feed = true;
long stepsWOData = 0;
String breakPoint = "NULL";



byte mac[] = {
0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

IPAddress ip(ARDUINO_IP_ADDR);


EthernetServer server(SYNCPORT);
EthernetServer feed_server(FEEDPORT);
boolean clientConnected = false;
boolean feed_clientConnected = false;

void httpServer_init(){
	Serial.begin(115200);
	// start the Ethernet connection and the server:
	Ethernet.begin(mac, ip);
	server.begin();
	feed_server.begin();
	Serial.print("server is at ");
	Serial.println(Ethernet.localIP());
	SPI.setClockDivider( SPI_CLOCK_DIV2 );
	// init break point array
	int i = 0;
	iopt_param_info* attrList;
	attrList = GET_TFIRED_INFO();
	for(i = 0; i < MODEL_N_TRANSITIONS; ++i){
		breakPoints[i].name = attrList[i].name;
		breakPoints[i].value = 0;
	}
	breakPoints[i].name = NULL;
	breakPoints[i].value = 0;
}

void httpServer_getRequest(){
	
	feed_cl = feed_server.available();
	
	if (feed_cl && feed_cl.connected())
	{
		feed_clientConnected = true;
	}else{
		first_feed = true;
		feed_clientConnected = false;
		stepsWOData = 0;
		feed_cl.stop();
		//Serial.println("Client disconnected");
	}
	cl = server.available();
	if (cl) {
		Serial.println("new client");
	
		while (cl.connected()) {
			clientConnected = true;
			if (cl.available()) {
				char c = cl.read();
				buffer = buffer + c;
				if (c == '\n') {
					ParseReceivedRequest(buffer, cl);
					break;
				}
			}
		}
	}
}

void httpServer_sendResponse(){
	// send a standard http response header
	if (feed_clientConnected == true)
	{
		sendFeedResponse(feed_cl);
	}
	
	if (clientConnected == true)
	{
		cl.write("HTTP/1.1 200 OK \n");
		cl.write("Content-Type: application/json\n");
		cl.write("Connection: close\n");  // the connection will be closed after completion of the response
		cl.write("Access-Control-Allow-Origin: *\n");
		cl.write("\n");
		Serial.println("sending sync msg response");
		
		if(cmd.equals(CMD_GET_INPUTS) )
		{
			cl.write("{");
			getInputResponse(cl);
			cl.write("}");
		}
                if(cmd.equals(CMD_SET_OUPUT)|| cmd.equals(CMD_SET_INPUT)){
                      
                      cl.print("{");
	              cl.print("\"result\":\"");
			cl.print("OK");
			cl.print("\"");
			cl.print("}");
                }
		else if(cmd.equals(CMD_GET_OUTPUTS)||cmd.equals(CMD_SET_OUPUT_ONCE))
		{
			cl.write("{");
			getOutputResponse(cl);
			cl.write("}");
		}
		else if(cmd.equals(CMD_GET_MARKING) || cmd.equals(CMD_SET_MARKING_ONCE))
		{
			cl.write("{");
			getMarkingResponse(cl);
			cl.write("}");
		}
		else if (cmd.equals(CMD_GET_ALL) || cmd.equals(CMD_SET_EXECSTEP))
		{
			getAll(cl);
		} 
		else if (cmd.equals(CMD_GET_MODELNAME))
		{
			cl.print("{");
			cl.print("\"model\":\"");
			cl.print(MODEL_NAME_STR);
			cl.print("\"");
			cl.print(",\"version\":\"");
			cl.print(MODEL_VERSION);
			cl.print("\"}");
		} 
		else if (cmd.equals(CMD_GET_TRANSFIRED))
		{
			cl.print("{");
			getTFiredResponse(cl);
			cl.print("}");
		}
		else if (cmd.equals(CMD_GET_TRANSITIONS))
		{
			cl.print("{");
			getTransitionsResponse(cl);
			cl.print("}");
		}
		else if (cmd.equals(CMD_SET_BREAKPOINT) || cmd.equals(CMD_GET_BREAKPOINT))
		{
			cl.print("{");
			getBreakPointResponse(cl);
			cl.print("}");
		}
		else if (cmd.equals(CMD_GET_TRACEMODE)||cmd.equals(CMD_SET_PAUSE)||cmd.equals(CMD_SET_START))
		{
			cl.print("{");
			cl.print("\"tm\":");
			cl.print(getTraceMode(trace_control));
			cl.print("}");
		}
		else if (cmd.equals(CMD_RESET))
		{
			cl.print("{");
				cl.print("\"Message\":");
				cl.print("\"The board will be reseted after 15MS\"");
			cl.print("}");
		}
		else if (cmd.equals(CMD_GET_FEEDURL))
		{
			cl.print("{");
			cl.print("\"url\":");
			//cl.print("\"http://");
			//cl.print(Ethernet.localIP());
			cl.print("\":");
			cl.print(FEEDPORT);
                        cl.print("\"");

			cl.print("}");
		}
		
		else if (cmd.equals(AUTH_FAIL))
		{
			cl.println("{\"error\":\"authentication failed\"}");
		}
		else
			cl.println("{\"error\":\"invalid request\"}");
				
	Serial.println("sending sync msg response complete");
	}
}

void ParseReceivedRequest(String &buffer, Client &c)
{
	int cmdStart = buffer.indexOf("/");
	if (cmdStart >= 0)
	{
		cmdStart += 1;
		int endCmd = buffer.indexOf("?");
		if (endCmd < 0)
		{
			endCmd = buffer.indexOf(" HTTP");
		}
		
		cmd = buffer.substring(cmdStart,endCmd);
		
		Serial.println("cmd = " + cmd);
		buffer = buffer.substring(endCmd+1, buffer.indexOf(" HTTP")) + "NULL";
		String pw = getParameter("pw", buffer);
		if (!pw.equals(PASSWORD))
		{
			cmd = AUTH_FAIL;
			httpServer_sendResponse();
			httpServer_disconnectClient();
		}else{
		
			if (cmd.equals(CMD_GET_MODELNAME))
			{
				httpServer_sendResponse();
				httpServer_disconnectClient();
			
			}else if (cmd.equals(CMD_GET_BREAKPOINT))
			{
				httpServer_sendResponse();
				httpServer_disconnectClient();
		
			}
			else if(cmd.equals(CMD_SET_INPUT))
			{
				setInput(buffer);
			}
			else if(cmd.equals(CMD_SET_OUPUT))
			{
				setOutput(buffer);
			}
			else if(cmd.equals(CMD_SET_OUPUT_ONCE))
			{
				setOutputs_once(buffer);
			}
			else if(cmd.equals(CMD_SET_MARKING_ONCE))
			{
				setMarking_once(buffer);
			}
			else if(cmd.equals(CMD_SET_BREAKPOINT))
			{
				setBreakPoint(buffer);
			}
			else if(cmd.equals(CMD_RESET))
			{
				httpServer_sendResponse();
				httpServer_disconnectClient();
				resetBoard();
			}
			else if (cmd.equals(CMD_SET_START))
			{
				trace_control = TRACE_CONT_RUN;
			}
			else if (cmd.equals(CMD_SET_PAUSE))
			{
				trace_control = TRACE_PAUSE;
			}
			else if (cmd.equals(CMD_SET_EXECSTEP))
			{
				String steps = getParameter(DS_STEPS, buffer);
				if (steps.equals(NO_PARM_FOUND))
				{
					trace_control = TRACE_SINGLE_STEP;
				}
				else
				{
					trace_control = TRACE_N_STEPS(steps.toInt());
					if (trace_control < 0)
					{
						trace_control = TRACE_PAUSE;
					}
				}
			}
		}
	}
}

void getAll(Client &c){
	c.print("{");
	getInputResponse(c);
	c.print(",");
	getOutputResponse(c);
	c.print(",");
	getMarkingResponse(c);
	c.print(",");
	getTransitionsResponse(c);
	c.print("}");
}

void getInputResponse(Client &c){
	
	c.print("\"");
	c.print(DS_INPUTS);
	c.print("\":{");
	//iopt_param_info* attrList = get_estagio2_InputInfo();
	iopt_param_info* attrList;
	attrList = GET_INPUT_INFO();
	getResponse(c, attrList, NULL);
	c.print("}");
}

void getOutputResponse(Client &c){
	c.print("\"");
	c.print(DS_OUTPUTS);
	c.print("\":{");
	iopt_param_info* attrList;
	attrList = GET_OUTPUT_INFO();
	getResponse(c, attrList, NULL);
	c.print("}");
}

void getMarkingResponse(Client &c){
	c.print("\"");
	c.print(DS_MARKINGS);
	c.print("\":{");
	iopt_param_info* attrList;
	attrList = GET_MARKING_INFO();
	getResponse(c, attrList, NULL);
	c.print("}");
}

void getTFiredResponse(Client &c){
	c.print("\"");
	c.print(DS_TRANSITIONS);
	c.print("\":{");
	iopt_param_info* attrList;
	attrList = GET_TFIRED_INFO();
	getResponse(c, attrList, 1);
	c.print("}");
}

void getTransitionsResponse(Client &c){
	c.print("\"");
	c.print(DS_TRANSITIONS);
	c.print("\":{");
	iopt_param_info* attrList;
	attrList = GET_TFIRED_INFO();
	getResponse(c, attrList, NULL);
	c.print("}");
}

void getBreakPointResponse(Client &c){
	c.print("\"");
	c.print(DS_BREAKPOINTS);
	c.print("\":{");
	getResponse(c, breakPoints, NULL);
	c.print("}");
}

void getResponse(Client &c, iopt_param_info* attrList, int filterValue)
{
	String response = "";
	int i = 0;
	for (i = 0; attrList[i].name != NULL; ++i)
	{
		if (filterValue && attrList[i].value != filterValue)
		{
			continue;
		}
		
		if (i > 0)
		{
			c.print(",");
			//response += ",";
		}
		
		c.print("\"");
		c.print(attrList[i].name);
		c.print("\":");
		c.print(attrList[i].value);
		/*response += "\"";
		response += attrList[i].name;
		response += "\":";
		response += attrList[i].value;*/
	}
	//return response;
}

String getTraceMode(int trace){
	if (trace == TRACE_CONT_RUN)
	{
		return "\"Running\"";
	} 
	else if (trace == TRACE_PAUSE)
	{
		return "\"Paused\"";
	} 
	else
	{
		return "\"StepByStep\"";
	}
}

String getParameter(String pName, String pList){
	int start = pList.indexOf(pName+"=");
	if (start > -1)
	{
		int endN = pList.indexOf("NULL");
		pList = pList.substring(start,endN);
		start = pList.indexOf("=");
		if (start > 0)
		{
			int end = pList.indexOf("&");
			if (end < 0)
			{
				end = endN;
			}
			return pList.substring(start+1,end);
		}
	}
	return NO_PARM_FOUND;
}

void setInput(String buffer){
	Serial.println("set input");
	iopt_param_info* aux_fv = new iopt_param_info[MODEL_N_INPUTS + 1];
		
	forceIO(buffer,aux_fv,GET_INPUT_INFO());
	
	if( input_fv != NULL )
		delete [] input_fv;
		
	input_fv = aux_fv;

}

void setOutput(String buffer){
	
	iopt_param_info* aux_fv = new iopt_param_info[MODEL_N_OUTPUTS + 1];
		
	forceIO(buffer,aux_fv,GET_OUTPUT_INFO());
	
	if( output_fv != NULL )
		delete [] output_fv;
	
	output_fv = aux_fv;
}

void setOutputs_once(String buffer)
{
	iopt_param_info* info, aux_fv[MODEL_N_OUTPUTS + 1];
	
	if(forceIO(buffer,aux_fv,GET_OUTPUT_INFO()) > 0){
		FORCE_OUTPUTS_VALUES(aux_fv, GET_PLACEOUT_PTR(), GET_EVTOUT_PTR());
	}
	int i = 0;
	info = GET_OUTPUT_INFO();
	for( i = 0; i < MODEL_N_OUTPUTS; ++i )
		lastOutputState[i] = info[i].value;
}

void setMarking_once(String buffer)
{
	iopt_param_info* info, aux_fv[MODEL_N_PLACES + 1];
	
	if(forceIO(buffer,aux_fv,GET_MARKING_INFO()) > 0){
		FORCE_MARKING_VALUES(aux_fv, GET_MARKING_PTR());
	}
	int i = 0;
	info = GET_MARKING_INFO();
	for( i = 0; i < MODEL_N_PLACES; ++i )
		lastMarkingState[i] = info[i].value;
}

void setBreakPoint(String buffer){
	int i = 0;
	String pValue = NO_PARM_FOUND;
	for (i = 0; i < MODEL_N_TRANSITIONS; ++i)
	{
		pValue = getParameter(breakPoints[i].name, buffer);
		if ((pValue != NO_PARM_FOUND))
		{
			breakPoints[i].value = pValue.toInt();
		}
	}
}

static int forceIO(String b,iopt_param_info fv[], iopt_param_info v[] )
{
	int n = 0;
	int i = 0;
	String pValue = NO_PARM_FOUND;
	for (i = 0; v[i].name; ++i)
	{
		pValue = getParameter(v[i].name, b);
		if (pValue != NO_PARM_FOUND)
		{
			fv[n].name = v[i].name;
			fv[n].value = pValue.toInt();
			n++;
			Serial.print("set value = ");
			Serial.println(v[i].name);
		}
	}
	fv[n].name = NULL;
	fv[n].value = 0;
	return n;
}

void httpServer_checkBreakPoints(){
	int i = 0;
	iopt_param_info* attrList;
	if( trace_control == TRACE_PAUSE ) return;
	attrList = GET_TFIRED_INFO();
	for (i = 0; i < MODEL_N_TRANSITIONS; ++i)
	{
		if (breakPoints[i].value == 1 && breakPoints[i].value == attrList[i].value)
		{
			trace_control = TRACE_PAUSE;
			breakPoint = breakPoints[i].name;
		}
	}
}

void httpServer_disconnectClient()
{
	
	if (clientConnected == true)
	{
		buffer = "";
		cl.stop();
		clientConnected = false;
		//Serial.println("client disconnected");
	}
}

void resetBoard()
{
	wdt_enable(WDTO_15MS);
	while(1)
	{
	}
}

void sendFeedResponse(Client &c){
	if (first_feed)
	{
		Serial.println("Sending Header");
		first_feed = false;
		c.print("HTTP/1.1 200 OK \n");
		c.print("Content-Type: text/event-stream\n");
		c.print("Access-Control-Allow-Origin: *\n");
		c.print("Cache-Control: no-cache \n");
		c.println();
		
		c.print("data:");
		getAll(c);
		c.println();
		c.println("\n\n");
		Serial.println("complete sending first msg");
	}else{

		boolean first_struct = true;
		iopt_param_info* attrList;
		attrList = GET_INPUT_INFO();
		getChangedSignals(c,lastInputState,attrList,MODEL_N_INPUTS,DS_INPUTS,first_struct);
		
		
		//Send outputs
		attrList = GET_OUTPUT_INFO();
		getChangedSignals(c,lastOutputState,attrList,MODEL_N_OUTPUTS,DS_OUTPUTS,first_struct);
		
		
		attrList = GET_MARKING_INFO();
		getChangedSignals(c,lastMarkingState,attrList,MODEL_N_PLACES,DS_MARKINGS,first_struct);
		
		
		attrList = GET_TFIRED_INFO();
		getChangedSignals(c,lastTransitionState,attrList,MODEL_N_TRANSITIONS,DS_TRANSITIONS,first_struct);
		
		sendTraceControl(c, first_struct);
		
		sendActiveBreakPoint(c, first_struct);
		
		if (!first_struct)
		{
			c.print(",\"");
			c.print(DS_STEPS);
			c.print("\":");
			c.print(stepsWOData);
			c.print("}");
			stepsWOData = 0;
			c.println();
			c.println();
			//Serial.println("Send complete");
		}else{
			if (stepsWOData >= 0 && (stepsWOData + 1) < 0)
			{
				Serial.println(stepsWOData);
			}
			
			stepsWOData += 1;;
			if (div(stepsWOData, STEPACK).rem == 0)
			{
				c.print("data:");
				c.print("{\"");
				c.print(DS_STEPS);
				c.print("\":");
				c.print(stepsWOData);
				c.print("}");
				c.println();
				c.println();
			}
		}
		
		//Serial.println("Send complete");
	}
}

void sendTraceControl(Client &c, boolean &first_struct)
{
	if (lastTraceControl != trace_control)
	{
		if (first_struct)
		{
			c.print("data:");
			c.print("{");
			first_struct = false;
		}else{
			c.print(",");
		}
		c.print("\"");
		c.print(DS_TRACECONTROL);
		c.print("\":");
		c.print(getTraceMode(trace_control));
		lastTraceControl = trace_control;
	}
}

void sendActiveBreakPoint(Client &c, boolean &first_struct)
{
	if (!breakPoint.equals("NULL"))
	{
		if (first_struct)
		{
			c.print("data:");
			c.print("{");
			first_struct = false;	
		}else{
			c.print(",");
		}
		c.print("\"");
		c.print(DS_ACTIVEBREAKPOINT);
		c.print("\":\"");
		c.print(breakPoint);
		c.print("\"");
		breakPoint = "NULL";
	}
}

void getChangedSignals(Client &c, int* lastState, iopt_param_info* newState, int len, String ds, boolean &first_struct)
{
	boolean first = true;
	int i = 0;
	for (i = 0; i < len; ++i)
	{
		if (lastState[i] != newState[i].value)
		{
			if (first)
			{
				if (first_struct)
				{
					c.print("data:");
					c.print("{");
					first_struct = false;
					
				}else{
					c.print(",");
				}
				
				c.print("\"");
				c.print(ds);
				c.print("\":{");
				first = false;
				
			}else{
				c.print(",");
			}	
			
			lastState[i] = newState[i].value;
			c.print("\"");
			c.print(newState[i].name);
			c.print("\":");
			c.print(newState[i].value);
		}
	}
	
	if (!first)
	{
		c.print("}");
	}
}

#endif
