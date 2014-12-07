/*
 * main.c - get weather details sample application
 *
 * Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
 *
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * Application Name     -   Get weather
 * Application Overview -   This is a sample application demonstrating how
 *                          to connect to openweathermap.org server and request
 *                          for weather details of a city. The application\
 *                          opens a TCP socket w/ the server and sends a HTTP
 *                          Get request to get the weather details. The received
 *                          data is processed and displayed on the console
 * Application Details  -   http://processors.wiki.ti.com/index.php/CC31xx_Get_Weather_Application
 *                          doc\examples\get_weather.pdf
 */

#include "simplelink.h"
#include "sl_common.h"
#include "convenienceFunctions.h"
#include "andrew_tempsense.h"
#include "data.h"
#include "Decode.h"

#define APPLICATION_VERSION "1.1.0"

#define SL_STOP_TIMEOUT        0xFF

#define CITY_NAME       "charlottesville"

//#define WEATHER_SERVER  "openweathermap.org"
#define WEATHER_SERVER  "capstone_ara.ngrok.com"
#define DEVICE_ID		"9000"

#define PREFIX_BUFFER   "GET /getcommand/134"
#define POST_BUFFER     " HTTP/1.1\r\nHost:capstone_ara.ngrok.com\r\nAccept: */"
#define POST_BUFFER2    "*\r\n\r\n"

#define GET_BUFFER		"GET /getcommand/"
#define TASK_PRE_BUFFER	"GET /completetask/device="
#define TASK_POST_BUFFER	"&task="
#define SEND_PRE_BUFFER 	"GET /sendtemp/device="
#define SEND_POST_BUFFER	"&temp="

#define SMALL_BUF           32
#define MAX_SEND_BUF_SIZE   512
#define MAX_SEND_RCV_SIZE   1024

/*Andrew's IR Defines */
#define REDLED		BIT0  // port 1 bit 0
#define GREENLED	BIT7  // port 4 bit 7
#define BUTTON 		BIT1  // port 2 bit 1
#define FREQ		25    // freq in MHz
#define PERIOD		320

#define IRLEDPORT	P3OUT
#define IRLEDDIR  	P3DIR
#define IRLED		BIT7  // port 3 bit 7 --> may need to change

/* Application specific status/error codes */
typedef enum{
    DEVICE_NOT_IN_STATION_MODE = -0x7D0,        /* Choosing this number to avoid overlap with host-driver's error codes */
    HTTP_SEND_ERROR = DEVICE_NOT_IN_STATION_MODE - 1,
    HTTP_RECV_ERROR = HTTP_SEND_ERROR - 1,
    HTTP_INVALID_RESPONSE = HTTP_RECV_ERROR -1,

    STATUS_CODE_MAX = -0xBB8
}e_AppStatusCodes;

//enum with possible cases for making network communication
typedef enum{
	GETCMD,
	MARKCOMPLETE,
	SEND
} tx_enum;

//enum with state machine states
typedef enum{
	INITIALIZE,
	RECVCMD,
	EXECCMD,
	SENDRESULT,
	READTEMP,
	SENDTEMP
} machine_states;

//Return codes for state machine operations.
typedef enum{
	INITCOMPLETE,
	NOCMD,
	RXFAIL,
	RXOK,
	EXECFAIL,
	EXECOK,
	TXOK,
	TXFAIL,
	READFAIL,
	READOK,
	ERRORSTATE
}stateMachineReturns;

//struct containing application data
typedef struct {
	machine_states state;
	stateMachineReturns returns;
}machineStatesReturns;


/*
 * GLOBAL VARIABLES -- Start
 */
_u32  g_Status = 0;
//Task struct which holds task ID and array of Longs.
IRCode currentTask;
struct{
    _u8 Recvbuff[MAX_SEND_RCV_SIZE];
    _u8 SendBuff[MAX_SEND_BUF_SIZE];

    _u8 HostName[SMALL_BUF];
    _u8 CityName[SMALL_BUF];

    _u32 DestinationIP;
    _i16 SockID;
    //hold the device ID string
    _u8 DeviceID[SMALL_BUF];
    //The command to be executed
    _i16 TaskID;
    //Most recente temperature reading
    _i16 RecentTemp;

}g_AppData;

machineStatesReturns thisMachine;

volatile int flag = 0;
/*
 * GLOBAL VARIABLES -- End
 */


/*
 * STATIC FUNCTION DEFINITIONS -- Start
 */
static _i32 establishConnectionWithAP();
static _i32 disconnectFromAP();
static _i32 configureSimpleLinkToDefaultState();

static _i32 initializeAppVariables();
static void  displayBanner();

static _i32 getWeather();
static _i32 getHostIP();
static _i32 createConnection();
static _i32 getData();

static _i32 sendMessage(tx_enum type, char* data);
static _i32 parseMessage(tx_enum type);

static _i32 config3100();
/*
 * STATIC FUNCTION DEFINITIONS -- End
 */
void manageStates(machine_states* state, stateMachineReturns* status);
void port_setup(void);
//void clock_setup(void);
void timer_setup(void);
void IRdelay(int delay);

void ir_transmit(long int code[], int size);

/*
 * ASYNCHRONOUS EVENT HANDLERS -- Start
 */

/*!
    \brief This function handles WLAN events

    \param[in]      pWlanEvent is the event passed to the handler

    \return         None

    \note

    \warning
*/
void SimpleLinkWlanEventHandler(SlWlanEvent_t *pWlanEvent)
{
    if(pWlanEvent == NULL)
        CLI_Write(" [WLAN EVENT] NULL Pointer Error \n\r");
    
    switch(pWlanEvent->Event)
    {
        case SL_WLAN_CONNECT_EVENT:
        {
            SET_STATUS_BIT(g_Status, STATUS_BIT_CONNECTION);

            /*
             * Information about the connected AP (like name, MAC etc) will be
             * available in 'slWlanConnectAsyncResponse_t' - Applications
             * can use it if required
             *
             * slWlanConnectAsyncResponse_t *pEventData = NULL;
             * pEventData = &pWlanEvent->EventData.STAandP2PModeWlanConnected;
             *
             */
        }
        break;

        case SL_WLAN_DISCONNECT_EVENT:
        {
            slWlanConnectAsyncResponse_t*  pEventData = NULL;

            CLR_STATUS_BIT(g_Status, STATUS_BIT_CONNECTION);
            CLR_STATUS_BIT(g_Status, STATUS_BIT_IP_ACQUIRED);

            pEventData = &pWlanEvent->EventData.STAandP2PModeDisconnected;

            /* If the user has initiated 'Disconnect' request, 'reason_code' is SL_USER_INITIATED_DISCONNECTION */
            if(SL_USER_INITIATED_DISCONNECTION == pEventData->reason_code)
            {
                CLI_Write(" Device disconnected from the AP on application's request \n\r");
            }
            else
            {
                CLI_Write(" Device disconnected from the AP on an ERROR..!! \n\r");
            }
        }
        break;

        default:
        {
            CLI_Write(" [WLAN EVENT] Unexpected event \n\r");
        }
        break;
    }
}

/*!
    \brief This function handles events for IP address acquisition via DHCP
           indication

    \param[in]      pNetAppEvent is the event passed to the handler

    \return         None

    \note

    \warning
*/
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent)
{
    if(pNetAppEvent == NULL)
        CLI_Write(" [NETAPP EVENT] NULL Pointer Error \n\r");
 
    switch(pNetAppEvent->Event)
    {
        case SL_NETAPP_IPV4_IPACQUIRED_EVENT:
        {
            SET_STATUS_BIT(g_Status, STATUS_BIT_IP_ACQUIRED);

            /*
             * Information about the connected AP's IP, gateway, DNS etc
             * will be available in 'SlIpV4AcquiredAsync_t' - Applications
             * can use it if required
             *
             * SlIpV4AcquiredAsync_t *pEventData = NULL;
             * pEventData = &pNetAppEvent->EventData.ipAcquiredV4;
             * <gateway_ip> = pEventData->gateway;
             *
             */
        }
        break;

        default:
        {
            CLI_Write(" [NETAPP EVENT] Unexpected event \n\r");
        }
        break;
    }
}

/*!
    \brief This function handles callback for the HTTP server events

    \param[in]      pHttpEvent - Contains the relevant event information
    \param[in]      pHttpResponse - Should be filled by the user with the
                    relevant response information

    \return         None

    \note

    \warning
*/
void SimpleLinkHttpServerCallback(SlHttpServerEvent_t *pHttpEvent,
                                  SlHttpServerResponse_t *pHttpResponse)
{
    /*
     * This application doesn't work with HTTP server - Hence these
     * events are not handled here
     */
    CLI_Write(" [HTTP EVENT] Unexpected event \n\r");
}

/*!
    \brief This function handles general error events indication

    \param[in]      pDevEvent is the event passed to the handler

    \return         None
*/
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent)
{
    /*
     * Most of the general errors are not FATAL are are to be handled
     * appropriately by the application
     */
    CLI_Write(" [GENERAL EVENT] \n\r");
}

/*!
    \brief This function handles socket events indication

    \param[in]      pSock is the event passed to the handler

    \return         None
*/
void SimpleLinkSockEventHandler(SlSockEvent_t *pSock)
{
    if(pSock == NULL)
        CLI_Write(" [SOCK EVENT] NULL Pointer Error \n\r");

    switch( pSock->Event )
    {
        case SL_SOCKET_TX_FAILED_EVENT:
        {
            /*
            * TX Failed
            *
            * Information about the socket descriptor and status will be
            * available in 'SlSockEventData_t' - Applications can use it if
            * required
            *
            * SlSockEventData_t *pEventData = NULL;
            * pEventData = & pSock->EventData;
            */
            switch( pSock->EventData.status )
            {
                case SL_ECLOSE:
                    CLI_Write(" [SOCK EVENT] Close socket operation failed to transmit all queued packets\n\r");
                break;


                default:
                    CLI_Write(" [SOCK EVENT] Unexpected event \n\r");
                break;
            }
        }
        break;

        default:
            CLI_Write(" [SOCK EVENT] Unexpected event \n\r");
        break;
    }
}
/*
 * ASYNCHRONOUS EVENT HANDLERS -- End
 */


/*
 * Application's entry point
 */
int main(int argc, char** argv)
{
    _i32 retVal = -1;

    /*Initialize App variables, including the state machine*/
    retVal = initializeAppVariables();
    ASSERT_ON_ERROR(retVal);

    /* Stop WDT and initialize the system-clock of the MCU */
    stopWDT();
    initClk();
    port_setup();
    timer_setup();
    ADCinit();
    _BIS_SR(GIE);	//Globally enable interrupts
//    while(1){
//    	sample();
//    	convert();
//    	_nop();
//    }

//    ir_transmit(bluetooth_power_toggle,71);
    _delay_cycles(1000);

    /* Configure command line interface */
    CLI_Configure();
    displayBanner();

    /*Underlying initialization has been completed, start using the state machine*/
    while(1){
    	manageStates(&thisMachine.state, &thisMachine.returns);
    }





    retVal = getWeather();

    if(retVal < 0)
    {
        CLI_Write(" Failed to get weather information \n\r");
        LOOP_FOREVER();
    }

    retVal = disconnectFromAP();
    if(retVal < 0)
    {
        CLI_Write(" Failed to disconnect from AP \n\r");
        LOOP_FOREVER();
    }

    return 0;
}

static _i32 config3100(){
	thisMachine.returns = ERRORSTATE;
    _i32 retVal = -1;
    /*
     * Following function configures the device to default state by cleaning
     * the persistent settings stored in NVMEM (viz. connection profiles &
     * policies, power policy etc)
     *
     * Applications may choose to skip this step if the developer is sure
     * that the device is in its default state at start of application
     *
     * Note that all profiles and persistent settings that were done on the
     * device will be lost
     */
    retVal = configureSimpleLinkToDefaultState();
    if(retVal < 0)
    {
        if (DEVICE_NOT_IN_STATION_MODE == retVal)
            CLI_Write(" Failed to configure the device in its default state \n\r");
        /*This should now be handled by the ERRORSTATE variable*/
//        LOOP_FOREVER();
        return;
    }

    CLI_Write(" Device is configured in default state \n\r");

    /*
     * Assumption is that the device is configured in station mode already
     * and it is in its default state
     */
    retVal = sl_Start(0, 0, 0);
    if ((retVal < 0) ||
        (ROLE_STA != retVal) )
    {
    	CLI_Write(" Failed to start the device \n\r");
        /*This should now be handled by the ERRORSTATE variable*/
//        LOOP_FOREVER();
    	return;
    }

    CLI_Write(" Device started as STATION \n\r");


    /* Connecting to WLAN AP */
    retVal = establishConnectionWithAP();
    if(retVal < 0)
    {
        CLI_Write(" Failed to establish connection w/ an AP \n\r");
        /*This should now be handled by the ERRORSTATE variable*/
//        LOOP_FOREVER();
        return;
    }

    CLI_Write(" Connection established w/ AP and IP is acquired \n\r");

    /*Indicate that we've completed the setup*/
    thisMachine.returns = INITCOMPLETE;
    return;
}

/*!
    \brief This function Obtains the required data from the server

    \param[in]      none

    \return         0 on success, -ve otherwise

    \note

    \warning
*/
static _i32 getData()
{
    _u8 *p_startPtr = NULL;
    _u8 *p_endPtr = NULL;
    _u8* p_bufLocation = NULL;
    _i32 retVal = -1;

    //first make our buffer just to test
    sendMessage(GETCMD, "H");

    pal_Memset(g_AppData.Recvbuff, 0, sizeof(g_AppData.Recvbuff));

    /* Debugging */
    CLI_Write("Send buffer is: \n\r");
    CLI_Write(g_AppData.SendBuff);
//    CLI_Write("\n\r");

    /* Send the HTTP GET string to the open TCP/IP socket. */
    retVal = sl_Send(g_AppData.SockID, g_AppData.SendBuff, pal_Strlen(g_AppData.SendBuff), 0);
    if(retVal != pal_Strlen(g_AppData.SendBuff))
        ASSERT_ON_ERROR(HTTP_SEND_ERROR);

    /* Receive response */
    retVal = sl_Recv(g_AppData.SockID, &g_AppData.Recvbuff[0], MAX_SEND_RCV_SIZE, 0);
    if(retVal <= 0)
        ASSERT_ON_ERROR(HTTP_RECV_ERROR);

    g_AppData.Recvbuff[pal_Strlen(g_AppData.Recvbuff)] = '\0';

    /*Debugging*/
    CLI_Write("The receive buffer contains: \n\r");
    CLI_Write(g_AppData.Recvbuff);
    CLI_Write("\n\r");

    parseMessage(GETCMD);

    /*Parse the Received Data*/
//    ir_transmit(bluetooth_power_toggle,71);
    ir_transmit(currentTask.pulses, currentTask.size);

    CLI_Write("Just tried to execute the command. \n\r");

    return SUCCESS;
}

//Adaptable sending method
static _i32 sendMessage(tx_enum type, char* data){
	//All methods use g_AppData.SendBuff, clear it first
	pal_Memset(g_AppData.SendBuff, 0, sizeof(g_AppData.SendBuff));

	//switch on which type: GETCMD, MARKCOMPLETE or SEND
	switch(type){
	case GETCMD:
		//build the string to make a GET request for a command
		pal_Strcat(g_AppData.SendBuff, GET_BUFFER);
		//Add in the device id
		pal_Strcat(g_AppData.SendBuff, DEVICE_ID);
		//Add in the post and post2 buffers
		pal_Strcat(g_AppData.SendBuff, POST_BUFFER);
		pal_Strcat(g_AppData.SendBuff, POST_BUFFER2);
		break;

	case MARKCOMPLETE:
		//Build a GET request that marks a command complete
		//In this case, data will contain an integer with completed task number
		pal_Strcat(g_AppData.SendBuff, TASK_PRE_BUFFER);
		pal_Strcat(g_AppData.SendBuff, DEVICE_ID);
		pal_Strcat(g_AppData.SendBuff, TASK_POST_BUFFER);
		//Append the task id
		char taskChars[SMALL_BUF];
		itoa(currentTask.id, taskChars);
		pal_Strcat(g_AppData.SendBuff, taskChars);
		pal_Strcat(g_AppData.SendBuff, POST_BUFFER);
        pal_Strcat(g_AppData.SendBuff, POST_BUFFER2);
        break;

    case SEND:
        //Send the temperature data point. Temp is an int. 
        pal_Strcat(g_AppData.SendBuff, SEND_PRE_BUFFER);
        pal_Strcat(g_AppData.SendBuff, DEVICE_ID);
        pal_Strcat(g_AppData.SendBuff, SEND_POST_BUFFER);
        //Convert temperature to a char and send. 
        char tempChars[SMALL_BUF];
        itoa(g_AppData.RecentTemp, tempChars);
        pal_Strcat(g_AppData.SendBuff, tempChars);
        pal_Strcat(g_AppData.SendBuff, POST_BUFFER);
		pal_Strcat(g_AppData.SendBuff, POST_BUFFER2);



		break;

	}
}

/* Adaptable Parsing Message */
static _i32 parseMessage(tx_enum type){
	  volatile char *p_messageStart = NULL;
	  _i32 returnVal = 0;
	switch(type){
	case GETCMD:{
		//Find beginning of message
		p_messageStart = pal_Strstr(g_AppData.Recvbuff, "start code:");
		if( NULL != p_messageStart ){
		    _nop();
		    volatile int len =  pal_Strlen("start code:");
			p_messageStart += len;
		    //Parse the code into the IRCode Struct
		    parseCode(&currentTask, (char*) p_messageStart);
		}
		else{
			CLI_Write("No IRCode detected");
		}
		returnVal = 1;
		break;
	}
	case MARKCOMPLETE:{
		break;
	}
	case SEND:{
		break;
	}
	default: {
		break;
	}
	}
	return returnVal;
}


/*!
    \brief Create TCP connection with openweathermap.org

    \param[in]      none

    \return         Socket descriptor for success otherwise negative

    \warning
*/
static _i32 createConnection()
{
    SlSockAddrIn_t  Addr;

    _i16 sd = 0;
    _i16 AddrSize = 0;
    _i32 ret_val = 0;

    Addr.sin_family = SL_AF_INET;
    Addr.sin_port = sl_Htons(80);

    /* Change the DestinationIP endianity, to big endian */
    Addr.sin_addr.s_addr = sl_Htonl(g_AppData.DestinationIP);

    AddrSize = sizeof(SlSockAddrIn_t);

    sd = sl_Socket(SL_AF_INET,SL_SOCK_STREAM, 0);
    if( sd < 0 )
    {
        CLI_Write((_u8 *)" Error creating socket\n\r\n\r");
        ASSERT_ON_ERROR(sd);
    }

    ret_val = sl_Connect(sd, ( SlSockAddr_t *)&Addr, AddrSize);
    if( ret_val < 0 )
    {
        /* error */
        CLI_Write((_u8 *)" Error connecting to server\n\r\n\r");
        ASSERT_ON_ERROR(ret_val);
    }

    return sd;
}

/*!
    \brief This function obtains the server IP address

    \param[in]      none

    \return         zero for success and -1 for error

    \warning
*/
static _i32 getHostIP()
{
    _i32 status = -1;

    status = sl_NetAppDnsGetHostByName((_i8 *)g_AppData.HostName,
                                       pal_Strlen(g_AppData.HostName),
                                       &g_AppData.DestinationIP, SL_AF_INET);
    /*Debugging*/
    CLI_Write("The host name from getHostIP is: \n\r");
    CLI_Write(g_AppData.HostName);
    CLI_Write("\n\r");

    ASSERT_ON_ERROR(status);

    return SUCCESS;
}

/*!
    \brief Get the Weather from server

    \param[in]      none

    \return         zero for success and -1 for error

    \warning
*/
static _i32 getWeather()
{
    _i32 retVal = -1;

    pal_Strcpy((char *)g_AppData.HostName, WEATHER_SERVER);

    retVal = getHostIP();
    if(retVal < 0)
    {
        CLI_Write((_u8 *)" Unable to reach Host\n\r\n\r");
        ASSERT_ON_ERROR(retVal);
    }

    g_AppData.SockID = createConnection();
    ASSERT_ON_ERROR(g_AppData.SockID);

    pal_Memset(g_AppData.CityName, 0x00, sizeof(g_AppData.CityName));
    pal_Memcpy(g_AppData.CityName, CITY_NAME, pal_Strlen(CITY_NAME));
    g_AppData.CityName[pal_Strlen(CITY_NAME)] = '\0';

    retVal = getData();
    ASSERT_ON_ERROR(retVal);


    retVal = sl_Close(g_AppData.SockID);
    ASSERT_ON_ERROR(retVal);

    return 0;
}

/*!
    \brief This function configure the SimpleLink device in its default state. It:
           - Sets the mode to STATION
           - Configures connection policy to Auto and AutoSmartConfig
           - Deletes all the stored profiles
           - Enables DHCP
           - Disables Scan policy
           - Sets Tx power to maximum
           - Sets power policy to normal
           - Unregisters mDNS services
           - Remove all filters

    \param[in]      none

    \return         On success, zero is returned. On error, negative is returned
*/
static _i32 configureSimpleLinkToDefaultState()
{
    SlVersionFull   ver = {0};
    _WlanRxFilterOperationCommandBuff_t  RxFilterIdMask = {0};

    _u8           val = 1;
    _u8           configOpt = 0;
    _u8           configLen = 0;
    _u8           power = 0;

    _i32          retVal = -1;
    _i32          mode = -1;

    mode = sl_Start(0, 0, 0);
    ASSERT_ON_ERROR(mode);

    /* If the device is not in station-mode, try configuring it in station-mode */
    if (ROLE_STA != mode)
    {
        if (ROLE_AP == mode)
        {
            /* If the device is in AP mode, we need to wait for this event before doing anything */
            while(!IS_IP_ACQUIRED(g_Status)) { _SlNonOsMainLoopTask(); }
        }

        /* Switch to STA role and restart */
        retVal = sl_WlanSetMode(ROLE_STA);
        ASSERT_ON_ERROR(retVal);

        retVal = sl_Stop(SL_STOP_TIMEOUT);
        ASSERT_ON_ERROR(retVal);

        retVal = sl_Start(0, 0, 0);
        ASSERT_ON_ERROR(retVal);

        /* Check if the device is in station again */
        if (ROLE_STA != retVal)
        {
            /* We don't want to proceed if the device is not coming up in station-mode */
            ASSERT_ON_ERROR(DEVICE_NOT_IN_STATION_MODE);
        }
    }

    /* Get the device's version-information */
    configOpt = SL_DEVICE_GENERAL_VERSION;
    configLen = sizeof(ver);
    retVal = sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION, &configOpt, &configLen, (_u8 *)(&ver));
    ASSERT_ON_ERROR(retVal);

    /* Set connection policy to Auto + SmartConfig (Device's default connection policy) */
    retVal = sl_WlanPolicySet(SL_POLICY_CONNECTION, SL_CONNECTION_POLICY(1, 0, 0, 0, 1), NULL, 0);
    ASSERT_ON_ERROR(retVal);

    /* Remove all profiles */
    retVal = sl_WlanProfileDel(0xFF);
    ASSERT_ON_ERROR(retVal);

    /*
     * Device in station-mode. Disconnect previous connection if any
     * The function returns 0 if 'Disconnected done', negative number if already disconnected
     * Wait for 'disconnection' event if 0 is returned, Ignore other return-codes
     */
    retVal = sl_WlanDisconnect();
    if(0 == retVal)
    {
        /* Wait */
        while(IS_CONNECTED(g_Status)) { _SlNonOsMainLoopTask(); }
    }

    /* Enable DHCP client*/
    retVal = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE,1,1,&val);
    ASSERT_ON_ERROR(retVal);

    /* Disable scan */
    configOpt = SL_SCAN_POLICY(0);
    retVal = sl_WlanPolicySet(SL_POLICY_SCAN , configOpt, NULL, 0);
    ASSERT_ON_ERROR(retVal);

    /* Set Tx power level for station mode
       Number between 0-15, as dB offset from max power - 0 will set maximum power */
    power = 0;
    retVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID, WLAN_GENERAL_PARAM_OPT_STA_TX_POWER, 1, (_u8 *)&power);
    ASSERT_ON_ERROR(retVal);

    /* Set PM policy to normal */
    retVal = sl_WlanPolicySet(SL_POLICY_PM , SL_NORMAL_POLICY, NULL, 0);
    ASSERT_ON_ERROR(retVal);

    /* Unregister mDNS services */
    retVal = sl_NetAppMDNSUnRegisterService(0, 0);
    ASSERT_ON_ERROR(retVal);

    /* Remove  all 64 filters (8*8) */
    pal_Memset(RxFilterIdMask.FilterIdMask, 0xFF, 8);
    retVal = sl_WlanRxFilterSet(SL_REMOVE_RX_FILTER, (_u8 *)&RxFilterIdMask,
                       sizeof(_WlanRxFilterOperationCommandBuff_t));
    ASSERT_ON_ERROR(retVal);

    retVal = sl_Stop(SL_STOP_TIMEOUT);
    ASSERT_ON_ERROR(retVal);

    retVal = initializeAppVariables();
    ASSERT_ON_ERROR(retVal);

    return retVal; /* Success */
}

/*!
    \brief Connecting to a WLAN Access point

    This function connects to the required AP (SSID_NAME).
    The function will return once we are connected and have acquired IP address

    \param[in]  None

    \return     0 on success, negative error-code on error

    \note

    \warning    If the WLAN connection fails or we don't acquire an IP address,
                We will be stuck in this function forever.
*/
static _i32 establishConnectionWithAP()
{
    SlSecParams_t secParams = {0};
    _i32 retVal = 0;
    //Austin Debugging here
//    secParams.Key = PASSKEY;
//    secParams.KeyLen = PASSKEY_LEN;
    secParams.Type = SEC_TYPE;

    retVal = sl_WlanConnect(SSID_NAME, pal_Strlen(SSID_NAME), 0, &secParams, 0);
    ASSERT_ON_ERROR(retVal);

    /* Wait */
    while((!IS_CONNECTED(g_Status)) || (!IS_IP_ACQUIRED(g_Status))) { _SlNonOsMainLoopTask(); }

    return SUCCESS;
}

/*!
    \brief Disconnecting from a WLAN Access point

    This function disconnects from the connected AP

    \param[in]      None

    \return         none

    \note

    \warning        If the WLAN disconnection fails, we will be stuck in this function forever.
*/
static _i32 disconnectFromAP()
{
    _i32 retVal = -1;

    /*
     * The function returns 0 if 'Disconnected done', negative number if already disconnected
     * Wait for 'disconnection' event if 0 is returned, Ignore other return-codes
     */
    retVal = sl_WlanDisconnect();
    if(0 == retVal)
    {
        /* Wait */
        while(IS_CONNECTED(g_Status)) { _SlNonOsMainLoopTask(); }
    }

    return SUCCESS;
}

/*!
    \brief This function initializes the application variables

    \param[in]  None

    \return     0 on success, negative error-code on error
*/
static _i32 initializeAppVariables()
{
    g_Status = 0;
    pal_Memset(&g_AppData, 0, sizeof(g_AppData));

    /*Austin - Set the device ID to its permanent value*/
//    g_AppData.DeviceID = 9000;
    g_AppData.TaskID = 7;
    g_AppData.RecentTemp = 55;

    thisMachine.returns = ERRORSTATE;
    thisMachine.state = INITIALIZE;

    return SUCCESS;
}

/*!
    \brief This function displays the application's banner

    \param      None

    \return     None
*/
static void displayBanner()
{
    CLI_Write("\n\r\n\r");
    CLI_Write(" AWESOME BLUETOOTH application - Version ");
    CLI_Write(APPLICATION_VERSION);
    CLI_Write("\n\r*******************************************************************************\n\r");
}


/*Andrew's code for IR Transmission */
void port_setup(void) {

	// Set up the Various LED's
	P1OUT &= ~REDLED;
	P4OUT &= ~GREENLED;
	IRLEDPORT &= ~IRLED;
	P1DIR |= REDLED;
	P4DIR |= GREENLED;
	IRLEDDIR |= IRLED;

	//set up the button
	P2REN |= BUTTON;
}

void timer_setup(void){
	TA0CCTL0 = CCIE | CM_0;		// enable capture
	TA0CCR0 = PERIOD;			// set the period
	TA0CTL = TASSEL_2 | ID_0 | TACLR | MC_1;  // source from SMLK, divide by 1, clear when hit limit, count up to CCR0
}

void ir_transmit(long int code[], int size) {
	volatile int i;
	flag = 0;
	for (i = 0; i < size; i++) {
		flag = 1-flag;  // flag toggle
		volatile long int delay = code[i];

		if (i > 65) {
			_nop();
		}

		if (delay > 30000) {
			_delay_cycles(37000*FREQ);
		}

		else if (flag == 1) {
			IRLEDPORT |= IRLED;
			_nop();
			IRdelay(delay);
		}

		else if (flag == 0){
			IRLEDPORT &= ~IRLED;
			_nop();
			IRdelay(delay);
		}



	}

	// turn off LED, stop transmission
	flag = 0;
	IRLEDPORT &= ~IRLED;
}

// delays for the IR transmit
// the int should be in micorseconds
void IRdelay(int delay) {
	// This will overshoot
	volatile int i = 0;
	for (i = 0; i < delay; i++){
		// assumes a 16 MHZ freq
		// subtract 7 to make up for incr, comparison, and function calls
		_delay_cycles(FREQ - 11);
	}
}


#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A(void) {	// when interrupt from A0 trips

	TA0CCTL0 &= ~CCIFG;		     // clear the flag
	if (flag==1) {
	    IRLEDPORT ^= IRLED;		// toggle the green led
	}
}

void manageStates(machine_states* state, stateMachineReturns* status){
	switch(*state){
		//Before manageStates is ever called, all the initialization functions should have been called
		case INITIALIZE:{
			/*Configure the CC3100 for WIFI connectivity*/
			config3100();
			switch(*status){
				case INITCOMPLETE:{
					//Change state to receive command
					//Nothing else to do here, setup for recieving command should have already been completed.
					*state = RECVCMD;
					break;
				}
				default:{
					*status = ERRORSTATE;
					break;
				}
			}
			break;
		}
		case RECVCMD:{
			//First attempt the network communication.
			//sendMessage must update status variable upon success.
//			sendMessage(GETCMD, "H");
//			recvMessage(GETCMD);
//			parseMessage(GETCMD);
			_nop();
			switch(*status){
				case RXOK:{
					*state = EXECCMD;
					break;
				}
				case RXFAIL:{
					*status = ERRORSTATE;
					break;
				}
			}
			break;
		}
		case EXECCMD:{
			//Attempt to broadcast the command. ir_transmit must set status variable upon success.
//			ir_transmit();
			switch(*status){
				case EXECOK:{
					*state = SENDRESULT;
					break;
				}
				case EXECFAIL:{
					*status = ERRORSTATE;
					break;
				}
				default:{
					*status = ERRORSTATE;
					break;
				}
			}
			break;
		}
		case SENDRESULT:{
			//Attempt to report completion of command
			//sendMessage must update status variable upon success.
//			sendMessage(MARKCOMPLETE, "H");
//			recvMessage(MARKCOMPLETE);
//			parseMessage(MARKCOMPLETE);
			switch(*status){
				case TXOK:{
					*state = READTEMP;
					break;
				}
				case RXFAIL:{
					*status = ERRORSTATE;
					break;
				}
				default:{
					*status = ERRORSTATE;
					break;
				}
			}
			break;
		}
		case READTEMP:{
			/*Attempt to read temperature. This could actually do the conversion
			 or just read the most recently converted value */
			/*readTemperature must set the status variable appropriately*/
//			readTemperature();
			switch(*status){
				case READOK:{
					*state = SENDTEMP;
					break;
				}
				case READFAIL:{
					*status = ERRORSTATE;
					break;
				}
				default:{
					*status = ERRORSTATE;
					break;
				}
			}
			break;
		}
		case SENDTEMP:{
			//Attempt to send the most recent temperature.
			//sendMessage or parseMessage must update status variable upon success.
//			sendMessage(SEND, "H");
//			recvMessage(SEND);
//			parseMessage(SEND);
			switch(*status){
				case TXOK:{
					*state = RECVCMD;
					break;
				}
				case TXFAIL:{
					*status = ERRORSTATE;
					break;
				}
				default:{
					*status = ERRORSTATE;
					break;
				}
			}
			break;
		}
		default:{
			*state = INITIALIZE;
			break;
		}
	}
}

