#include "LED.h"
#include "ledcontrol.h"

#include "MemManager.h"
#include "Messaging.h"
#include "TimersManager.h"
#include "SecLib.h"
#include "Panic.h"
#include "fsl_xcvr.h"
#include "fsl_os_abstraction.h"
#include "SerialManager.h"

#include "board.h"

#include "FreeRTOSConfig.h"


#define App_NotifySelf() OSA_EventSet(mAppThreadEvt, gCtEvtSelfEvent_c)

/*Application main*/
static void App_Thread (uint32_t param); 
/*Application event handler*/
static void App_HandleEvents(osaEventFlags_t flags);
/*Function that reads latest byte from Serial Manager*/
static void App_UpdateUartData(uint8_t* pData);


/*Generic FSK RX callback*/
static void App_GenFskReceiveCallback(uint8_t *pBuffer, 
                                      uint16_t bufferLength, 
                                      uint64_t timestamp, 
                                      uint8_t rssi,
                                      uint8_t crcValid);
/*Generic FSK Notification callback*/
static void App_GenFskEventNotificationCallback(genfskEvent_t event, 
                                                genfskEventStatus_t eventStatus);
/*Serial Manager UART RX callback*/
static void App_SerialCallback(void* param);

/*Timer manager callback function*/
static void App_TimerCallback(void* param);


//application specific genfsk init
static void gFsk_Init();


/************************************************************************************
*************************************************************************************
* Private memory declarations
*************************************************************************************
************************************************************************************/
static uint8_t platformInitialized = 0;

/*event used by the application thread*/
static osaEventId_t mAppThreadEvt;

#ifdef LEDCONTROL_MASTER
/*variable to store key pressed by user*/
static uint8_t mAppUartData = 0;
static bool timerInit = false;
#endif


//transmit or receive state
static app_states_t appState = gAppRx_c;

#ifdef LEDCONTROL_MASTER
static connectivity_states_t conState = gAppSlave1Check;
#endif

/*structure to store information regarding latest received packet*/
static ct_rx_indication_t mAppRxLatestPacket;

/*latest generic fsk event status*/
static genfskEventStatus_t mAppGenfskStatus;

// transmission buffer and packet
static uint8_t* gTxBuffer;
static GENFSK_packet_t gTxPacket;

//receive buffer/packet
static uint8_t* gRxBuffer;
static GENFSK_packet_t gRxPacket;

//length of genfsk buffer
uint16_t buffLen;

/*extern MCU reset api*/
extern void ResetMCU(void);


/*! *********************************************************************************
* \brief  This is the first task created by the OS. This task will initialize 
*         the system
*
* \param[in]  param
*
********************************************************************************** */
void main_task(uint32_t param)
{  
    if (!platformInitialized)
    {
        platformInitialized = 1;
        
        hardware_init();
        
        /* Framework init */
        MEM_Init();

        //initialize Serial Manager
        SerialManager_Init();
        LED_Init();
        SecLib_Init();
#ifdef LEDCONTROL_MASTER
        TMR_Init();
#endif

        GENFSK_Init();
        
        mAppTmrId = TMR_AllocateTimer();

        
        /*create app thread event*/
        mAppThreadEvt = OSA_EventCreate(TRUE);
        

        /*initialize the application interface id*/
        Serial_InitInterface(&mAppSerId, 
                             APP_SERIAL_INTERFACE_TYPE, 
                             APP_SERIAL_INTERFACE_INSTANCE);
        /*set baudrate to 115200*/
        Serial_SetBaudRate(mAppSerId, 
                           APP_SERIAL_INTERFACE_SPEED);
        /*set Serial Manager receive callback*/
        Serial_SetRxCallBack(mAppSerId, App_SerialCallback, NULL);
        



        /* GENFSK LL Init with default register config */
        if(gGenfskSuccess_c != GENFSK_AllocInstance(&mAppGenfskId, NULL, NULL, NULL))
        {
        	Serial_Print(mAppGenfskId,"Allocation failed...\r\n",gAllowToBlock_d);
        }

    }
    
    /* Call application task */
    App_Thread( param );
}

/*! *********************************************************************************
* \brief  This function represents the Application task. 
*         This task reuses the stack alocated for the MainThread.
*         This function is called to process all events for the task. Events 
*         include timers, messages and any other user defined events.
* \param[in]  argument
*
********************************************************************************** */
void App_Thread (uint32_t param)
{
    osaEventFlags_t mAppThreadEvtFlags = 0;
    
    gFsk_Init();
#ifdef LEDCONTROL_MASTER
    TMR_EnableTimer(mAppTmrId);
    TMR_StartSingleShotTimer(mAppTmrId,10, App_TimerCallback, NULL);
#endif
    while(1)
    {
    	switch(appState)
    	{
    	case gAppRx_c:
    		GENFSK_AbortAll();
    		GENFSK_StartRx(mAppGenfskId, gRxBuffer, gGenFskDefaultMaxBufferSize_c+crcConfig.crcSize, 0, 0);
    		break;
    	case gAppTx_c:
    		GENFSK_AbortAll();
    		GENFSK_StartTx(mAppGenfskId,gTxBuffer,buffLen,0);
    		break;
    	default:
    		GENFSK_AbortAll();
    		appState = gAppRx_c;
    		break;

    	}
        (void)OSA_EventWait(mAppThreadEvt, gCtEvtEventsAll_c, FALSE, osaWaitForever_c ,&mAppThreadEvtFlags);
        if(mAppThreadEvtFlags)
        {
        	App_HandleEvents(mAppThreadEvtFlags);/*handle app events*/
        }
    }
}



/*! *********************************************************************************
* \brief  The application event handler 
*         This function is called each time there is an OS event for the AppThread
* \param[in]  flags The OS event flags specific to the Connectivity Test App.
*
********************************************************************************** */
void App_HandleEvents(osaEventFlags_t flags)
{
    if(flags & gCtEvtUart_c)
    {
#ifdef LEDCONTROL_MASTER //slave boards should never fire a uart event
        App_UpdateUartData(&mAppUartData);

		if(
				  (mAppUartData != '0')
				&&(mAppUartData != '1')
				&&(mAppUartData != '2')
				&&(mAppUartData != '3')
				&&(mAppUartData != '4')
				&&(mAppUartData != '5')
				&&(mAppUartData != '6')
				&&(mAppUartData != '7')
				&&(mAppUartData != '8')
		  )
		{
			appState = gAppRx_c;//incorrect character sent, ignore
		}
		else
		{
			int command = mAppUartData - '0';//convert to int
			gTxPacket.header.lengthField = gGenFskMinPayloadLen_c;
			switch(command)
			{
			case 0:
				//Device ID 0, red LED
				gTxPacket.payload[0] = LEDCONTROL_DEVICE_ID_ZERO;
				gTxPacket.payload[1] = 'r';
				break;
			case 1:
				//Device ID 0, green LED
				gTxPacket.payload[0] = LEDCONTROL_DEVICE_ID_ZERO;
				gTxPacket.payload[1] = 'g';
				break;
			case 2:
				//Device ID 0, blue LED
				gTxPacket.payload[0] = LEDCONTROL_DEVICE_ID_ZERO;
				gTxPacket.payload[1] = 'b';
				break;
			case 3:
				//Device ID 1, red LED
				gTxPacket.payload[0] = LEDCONTROL_DEVICE_ID_ONE;
				gTxPacket.payload[1] = 'r';
				break;
			case 4:
				//Device ID 1, green LED
				gTxPacket.payload[0] = LEDCONTROL_DEVICE_ID_ONE;
				gTxPacket.payload[1] = 'g';
				break;
			case 5:
				//Device ID 1, blue LED
				gTxPacket.payload[0] = LEDCONTROL_DEVICE_ID_ONE;
				gTxPacket.payload[1] = 'b';
				break;
			case 6:
				//Device ID 2, red LED
				gTxPacket.payload[0] = LEDCONTROL_DEVICE_ID_TWO;
				gTxPacket.payload[1] = 'r';
				break;
			case 7:
				//Device ID 2, green LED
				gTxPacket.payload[0] = LEDCONTROL_DEVICE_ID_TWO;
				gTxPacket.payload[1] = 'g';
				break;
			case 8:
				//Device ID 2, blue LED
				gTxPacket.payload[0] = LEDCONTROL_DEVICE_ID_TWO;
				gTxPacket.payload[1] = 'b';
				break;
			default:
				break;

				buffLen = gTxPacket.header.lengthField+(gGenFskDefaultHeaderSizeBytes_c)+(gGenFskDefaultSyncAddrSize_c + 1);
				GENFSK_PacketToByteArray(mAppGenfskId, &gTxPacket, gTxBuffer);
				appState = gAppTx_c;
			}
		}
#endif
    }
    else if(flags & gCtEvtRxDone_c) //received comms
    {
    	GENFSK_ByteArrayToPacket(mAppGenfskId, mAppRxLatestPacket.pBuffer, &gRxPacket);
    	uint8_t devID = gRxPacket.payload[0];
    	uint8_t data = gRxPacket.payload[1];
#ifdef LEDCONTROL_MASTER

    	if(data == 3)
    	{
    		if(
    				  ((conState == gAppSlave1Check) && (devID = LEDCONTROL_DEVICE_ID_ZERO))
					||((conState == gAppSlave2Check) && (devID = LEDCONTROL_DEVICE_ID_ONE))
					||((conState == gAppSlave3Check) && (devID = LEDCONTROL_DEVICE_ID_TWO))
			  )
    		{
    			//successful reply from connectivity check, stop timer and start new check
    			Serial_Print(mAppSerId,"Connectivity check successful",gAllowToBlock_d);
    			TMR_StopTimer(mAppTmrId);
    			if(conState == gAppSlave1Check)
    			{
    				conState = gAppSlave2Check;
    			}
    			else if(conState == gAppSlave2Check)
    			{
    				conState = gAppSlave3Check;
    			}
    			else if(conState == gAppSlave3Check)
    			{
    				conState = gAppSlave1Check;
    			}
    			else
    			{
    				//unreachable
    			}
    			gTxPacket.payload[1] = 3;
    			if(conState == gAppSlave1Check)
    			{
    				gTxPacket.payload[0] = LEDCONTROL_DEVICE_ID_ZERO;
    			}
    			else if(conState == gAppSlave2Check)
    			{
    				gTxPacket.payload[0] = LEDCONTROL_DEVICE_ID_ONE;
    			}
    			else if(conState == gAppSlave3Check)
    			{
    				gTxPacket.payload[0] = LEDCONTROL_DEVICE_ID_TWO;
    			}
    			else
    			{
    				//unreachable
    			}
				buffLen = gTxPacket.header.lengthField+(gGenFskDefaultHeaderSizeBytes_c)+(gGenFskDefaultSyncAddrSize_c + 1);
				GENFSK_PacketToByteArray(mAppGenfskId, &gTxPacket, gTxBuffer);
				appState = gAppTx_c;

    		}
    		else
    		{
    			//bad packet, ignore
    			appState = gAppRx_c;
    		}
    	}
    	else
    	{
        	//TODO: Print suitable data back to indicate successful slave reception of data
        	appState = gAppRx_c;
    	}


#else
    	Serial_Print(mAppSerId,"Received a packet!\r\n",gAllowToBlock_d);
    	if(devID == LEDCONTROL_DEVICE_ID)
    	{
    		gTxPacket.payload[0] = LEDCONTROL_DEVICE_ID;
			if(data == 0)
			{
				Led2Toggle();
				gTxPacket.payload[1] = 'r';
			}
			else if(data == 1)
			{
				Led3Toggle();
				gTxPacket.payload[1] = 'g';
			}
			else if(data == 2)
			{
				Led4Toggle();
				gTxPacket.payload[1] = 'b';
			}
			else if(data == 3)
			{
				//this packet is so the master can check slave is still connected, send response back
				gTxPacket.payload[1] = 'y';
			}
			else{}
			buffLen = gTxPacket.header.lengthField+(gGenFskDefaultHeaderSizeBytes_c)+(gGenFskDefaultSyncAddrSize_c + 1);
			GENFSK_PacketToByteArray(mAppGenfskId, &gTxPacket, gTxBuffer);
			appState = gAppTx_c;
    	}
#endif
    }
    else if(flags & gCtEvtTxDone_c)
    {

#ifdef LEDCONTROL_MASTER
    	if(gTxPacket.payload[1] == 3)//if last packet was a connection check
    	{
    		TMR_StartSingleShotTimer(mAppTmrId, LEDCONTROL_CONNECTIONCHECK_TIMEOUT_MILLISECONDS, App_TimerCallback, NULL);
    	}
    	else
    	{
    		Serial_Print(mAppSerId,"Last packet not a connection check\r\n",gAllowToBlock_d);
    	}
#endif
    	appState = gAppRx_c;
    }
    else if(flags & gCtEvtTimerExpired_c)
    {
#ifdef LEDCONTROL_MASTER
    	if(timerInit)
    	{
    		TMR_StopTimer(mAppTmrId);

			switch(conState)
			{
			case gAppSlave1Check:
				Serial_Print(mAppSerId,"Test failed for slave 1; disconnect\r\n",gNoBlock_d);
				break;
			case gAppSlave2Check:
				Serial_Print(mAppSerId,"Test failed for slave 2; disconnect\r\n",gNoBlock_d);
				break;
			case gAppSlave3Check:
				Serial_Print(mAppSerId,"Test failed for slave 3; disconnect\r\n",gNoBlock_d);
				break;
			default: break;
			}
			if(conState == gAppSlave1Check)
			{
				conState = gAppSlave2Check;
			}
			else if(conState == gAppSlave2Check)
			{
				conState = gAppSlave3Check;
			}
			else if(conState == gAppSlave3Check)
			{
				conState = gAppSlave1Check;
			}
			else
			{
				//unreachable
			}
			gTxPacket.payload[1] = 3;
			if(conState == gAppSlave1Check)
			{
				gTxPacket.payload[0] = LEDCONTROL_DEVICE_ID_ZERO;
			}
			else if(conState == gAppSlave2Check)
			{
				gTxPacket.payload[0] = LEDCONTROL_DEVICE_ID_ONE;
			}
			else if(conState == gAppSlave3Check)
			{
				gTxPacket.payload[0] = LEDCONTROL_DEVICE_ID_TWO;
			}
			else
			{
				//unreachable
			}
			buffLen = gTxPacket.header.lengthField+(gGenFskDefaultHeaderSizeBytes_c)+(gGenFskDefaultSyncAddrSize_c + 1);
			GENFSK_PacketToByteArray(mAppGenfskId, &gTxPacket, gTxBuffer);
			appState = gAppTx_c;

    	}
    	else
    	{
			gTxPacket.payload[1] = 3;
			gTxPacket.payload[0] = LEDCONTROL_DEVICE_ID_ZERO;
    		timerInit = true;
			buffLen = gTxPacket.header.lengthField+(gGenFskDefaultHeaderSizeBytes_c)+(gGenFskDefaultSyncAddrSize_c + 1);
			GENFSK_PacketToByteArray(mAppGenfskId, &gTxPacket, gTxBuffer);
			appState = gAppTx_c;
    	}
#endif
    }
    else
    {
    	App_NotifySelf();
    }

}

/*! *********************************************************************************
* \brief  This function is called each time SerialManager notifies the application
*         task that a byte was received.
*         The function checks if there are additional bytes in the SerialMgr  
*         queue and simulates a new SM event if there is more data.
* \param[in]  pData Pointer to where to store byte read.
*
********************************************************************************** */
static void App_UpdateUartData(uint8_t* pData)
{
    uint16_t u16SerBytesCount = 0;
    if(gSerial_Success_c == Serial_GetByteFromRxBuffer(mAppSerId, pData, &u16SerBytesCount))
    {
        Serial_RxBufferByteCount(mAppSerId, &u16SerBytesCount);
        if(u16SerBytesCount)
        {
            (void)OSA_EventSet(mAppThreadEvt, gCtEvtUart_c);
        }
    } 
}



/*! *********************************************************************************
* \brief  This function represents the Generic FSK receive callback. 
*         This function is called each time the Generic FSK Link Layer receives a 
*         valid packet
* \param[in]  pBuffer Pointer to receive buffer as byte array
* \param[in]  timestamp Generic FSK timestamp for received packet
* \param[in]  rssi The RSSI measured during the reception of the packet
*
********************************************************************************** */
static void App_GenFskReceiveCallback(uint8_t *pBuffer, 
                                      uint16_t bufferLength, 
                                      uint64_t timestamp, 
                                      uint8_t rssi,
                                      uint8_t crcValid)
{
   mAppRxLatestPacket.pBuffer      = pBuffer;
   mAppRxLatestPacket.bufferLength = bufferLength;
   mAppRxLatestPacket.timestamp    = timestamp;
   mAppRxLatestPacket.rssi         = rssi;
   mAppRxLatestPacket.crcValid     = crcValid;
   
   /*send event to app thread*/
   OSA_EventSet(mAppThreadEvt, gCtEvtRxDone_c);
}

/*! *********************************************************************************
* \brief  This function represents the Generic FSK event notification callback. 
*         This function is called each time the Generic FSK Link Layer has 
*         a notification for the upper layer
* \param[in]  event The event that generated the notification
* \param[in]  eventStatus status of the event
*
********************************************************************************** */
static void App_GenFskEventNotificationCallback(genfskEvent_t event, 
                                                genfskEventStatus_t eventStatus)
{
   if(event & gGenfskTxEvent)
   {
       mAppGenfskStatus = eventStatus;
       /*send event done*/
       OSA_EventSet(mAppThreadEvt, gCtEvtTxDone_c);
   }
   if(event & gGenfskRxEvent)
   {
       if(eventStatus == gGenfskTimeout)
       {
           OSA_EventSet(mAppThreadEvt, gCtEvtSeqTimeout_c);
       }
       else
       {
           OSA_EventSet(mAppThreadEvt, gCtEvtRxFailed_c);
       }
   }
}

static void App_SerialCallback(void* param)
{
    OSA_EventSet(mAppThreadEvt, gCtEvtUart_c);
}


static void gFsk_Init()
{
	GENFSK_RegisterCallbacks(mAppGenfskId, App_GenFskReceiveCallback, App_GenFskEventNotificationCallback);
    gRxBuffer  = MEM_BufferAlloc(gGenFskDefaultMaxBufferSize_c +
                                 crcConfig.crcSize);
    gTxBuffer  = MEM_BufferAlloc(gGenFskDefaultMaxBufferSize_c);

    gRxPacket.payload = (uint8_t*)MEM_BufferAlloc(gGenFskMaxPayloadLen_c  +
                                                       crcConfig.crcSize);
    gTxPacket.payload = (uint8_t*)MEM_BufferAlloc(gGenFskMaxPayloadLen_c);

    /*prepare the part of the tx packet that is common for all tests*/
    gTxPacket.addr = gGenFskDefaultSyncAddress_c;
    gTxPacket.header.h0Field = gGenFskDefaultH0Value_c;
    gTxPacket.header.h1Field = gGenFskDefaultH1Value_c;

    /*set bitrate*/
    GENFSK_RadioConfig(mAppGenfskId, &radioConfig);
    /*set packet config*/
    GENFSK_SetPacketConfig(mAppGenfskId, &pktConfig);
    /*set whitener config*/
    GENFSK_SetWhitenerConfig(mAppGenfskId, &whitenConfig);
    /*set crc config*/
    GENFSK_SetCrcConfig(mAppGenfskId, &crcConfig);

    /*set network address at location 0 and enable it*/
    GENFSK_SetNetworkAddress(mAppGenfskId, 0, &ntwkAddr);
    GENFSK_EnableNetworkAddress(mAppGenfskId, 0);

    /*set tx power level*/
    GENFSK_SetTxPowerLevel(mAppGenfskId, gGenFskDefaultTxPowerLevel_c);
    /*set channel: Freq = 2360MHz + ChannNumber*1MHz*/
    GENFSK_SetChannelNumber(mAppGenfskId, gGenFskDefaultChannel_c);
}

static void App_TimerCallback(void* param)
{
    OSA_EventSet(mAppThreadEvt, gCtEvtTimerExpired_c);
}



