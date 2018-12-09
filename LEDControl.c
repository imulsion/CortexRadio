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


/*variable to store key pressed by user*/
static uint8_t mAppUartData = 0;




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
        mAppTmrId = TMR_AllocateTimer();

#endif

        GENFSK_Init();
        


        
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
    TMR_StartIntervalTimer(mAppTmrId,LEDCONTROL_CONNECTIONCHECK_TIMEOUT_MILLISECONDS, App_TimerCallback, NULL);
#endif
    GENFSK_StartRx(mAppGenfskId, gRxBuffer, gGenFskDefaultMaxBufferSize_c+crcConfig.crcSize, 0, 0);
    while(1)
    {
        (void)OSA_EventWait(mAppThreadEvt, gCtEvtEventsAll_c, FALSE, osaWaitForever_c ,&mAppThreadEvtFlags);
        if(mAppThreadEvtFlags)
        {
        	App_HandleEvents(mAppThreadEvtFlags);/*handle app events*/
        }
    }
}

void App_HandleEvents(osaEventFlags_t flags)
{
    if(flags & gCtEvtUart_c)
    {
        App_UpdateUartData(&mAppUartData);



		if(
				  (mAppUartData != '1')
				&&(mAppUartData != '2')
				&&(mAppUartData != '3')
				&&(mAppUartData != '4')
				&&(mAppUartData != '5')
				&&(mAppUartData != '6')
				&&(mAppUartData != '7')
				&&(mAppUartData != '8')
				&&(mAppUartData != '9')
		  )
		{
			GENFSK_AbortAll();
			GENFSK_StartRx(mAppGenfskId, gRxBuffer, gGenFskDefaultMaxBufferSize_c+crcConfig.crcSize, 0, 0);
		}
		else
		{
			int command = (mAppUartData - '0')-1;//convert to int
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
				GENFSK_AbortAll();
				GENFSK_StartTx(mAppGenfskId, gTxBuffer, buffLen, 0);
			}
		}
    }
    else if(flags & gCtEvtRxDone_c) //received comms
    {

    	GENFSK_ByteArrayToPacket(mAppGenfskId, mAppRxLatestPacket.pBuffer, &gRxPacket);
    	uint8_t devID = gRxPacket.payload[0];
    	uint8_t data = gRxPacket.payload[1];
#ifdef LEDCONTROL_MASTER

    	if(data == 'v')
    	{
    		if((conState == gAppSlave1Check) && (devID = LEDCONTROL_DEVICE_ID_ZERO))
    		{
    			slave1connected = true;
    		}
    		else if((conState == gAppSlave2Check) && (devID = LEDCONTROL_DEVICE_ID_ONE))
    		{
    			slave2connected = true;
    		}
    		else if((conState == gAppSlave3Check) && (devID = LEDCONTROL_DEVICE_ID_TWO))
    		{
    			slave3connected = true;
    		}
    		else
    		{
    			//bad packet, ignore
    		}
    	}
    	else
    	{
        	//TODO: Print suitable data back to indicate successful slave reception of data
    		if(devID == 0)
    		{
    			if(data == 'r')
    			{
    				Serial_Print(mAppSerId,"1",gAllowToBlock_d);
    			}
    			else if(data == 'g')
    			{
    				Serial_Print(mAppSerId,"2",gAllowToBlock_d);
    			}
    			else if(data == 'b')
    			{
    				Serial_Print(mAppSerId,"3",gAllowToBlock_d);
    			}
    			else
    			{
    				//bad data
    			}
    		}
    		else if(devID == 1)
    		{
    			if(data == 'r')
    			{
    				Serial_Print(mAppSerId,"4",gAllowToBlock_d);
    			}
    			else if(data == 'g')
    			{
    				Serial_Print(mAppSerId,"5",gAllowToBlock_d);
    			}
    			else if(data == 'b')
    			{
    				Serial_Print(mAppSerId,"6",gAllowToBlock_d);
    			}
    			else
    			{
    				//bad data
    			}
    		}
    		else if(devID == 2)
    		{
    			if(data == 'r')
    			{
    				Serial_Print(mAppSerId,"7",gAllowToBlock_d);
    			}
    			else if(data == 'g')
    			{
    				Serial_Print(mAppSerId,"8",gAllowToBlock_d);
    			}
    			else if(data == 'b')
    			{
    				Serial_Print(mAppSerId,"9",gAllowToBlock_d);
    			}
    			else
    			{
    				//bad data
    			}
    		}
    		else
    		{
    			//bad data
    		}
    		GENFSK_AbortAll();
			GENFSK_StartRx(mAppGenfskId, gRxBuffer, gGenFskDefaultMaxBufferSize_c+crcConfig.crcSize, 0, 0);

    	}


#else
    	if(devID == LEDCONTROL_DEVICE_ID)
    	{
    		gTxPacket.payload[0] = LEDCONTROL_DEVICE_ID;

			if(data == 'r')
			{
				Led2Toggle();
				gTxPacket.payload[1] = 'r';
				buffLen = gTxPacket.header.lengthField+(gGenFskDefaultHeaderSizeBytes_c)+(gGenFskDefaultSyncAddrSize_c + 1);
				GENFSK_PacketToByteArray(mAppGenfskId, &gTxPacket, gTxBuffer);
				GENFSK_AbortAll();
				GENFSK_StartTx(mAppGenfskId, gTxBuffer, buffLen, 0);
			}
			else if(data == 'g')
			{
				Led3Toggle();
				gTxPacket.payload[1] = 'g';
				buffLen = gTxPacket.header.lengthField+(gGenFskDefaultHeaderSizeBytes_c)+(gGenFskDefaultSyncAddrSize_c + 1);
				GENFSK_PacketToByteArray(mAppGenfskId, &gTxPacket, gTxBuffer);
				GENFSK_AbortAll();
				GENFSK_StartTx(mAppGenfskId, gTxBuffer, buffLen, 0);
			}
			else if(data == 'b')
			{
				Led4Toggle();
				gTxPacket.payload[1] = 'b';
				buffLen = gTxPacket.header.lengthField+(gGenFskDefaultHeaderSizeBytes_c)+(gGenFskDefaultSyncAddrSize_c + 1);
				GENFSK_PacketToByteArray(mAppGenfskId, &gTxPacket, gTxBuffer);
				GENFSK_AbortAll();
				GENFSK_StartTx(mAppGenfskId, gTxBuffer, buffLen, 0);
			}
			else if(data == 'v')
			{
				//this packet is so the master can check slave is still connected, send response back
	    		Serial_Print(mAppSerId,"Right place\r\n",gAllowToBlock_d);
				gTxPacket.payload[1] = 'v';
				buffLen = gTxPacket.header.lengthField+(gGenFskDefaultHeaderSizeBytes_c)+(gGenFskDefaultSyncAddrSize_c + 1);
				GENFSK_PacketToByteArray(mAppGenfskId, &gTxPacket, gTxBuffer);
				GENFSK_AbortAll();
				GENFSK_StartTx(mAppGenfskId, gTxBuffer, buffLen, 0);
			}
			else
			{
				//bad data
				Serial_Print(mAppSerId,"Bad data\r\n",gAllowToBlock_d);
				GENFSK_AbortAll();
				GENFSK_StartRx(mAppGenfskId, gRxBuffer, gGenFskDefaultMaxBufferSize_c+crcConfig.crcSize, 0, 0);
			}

    	}
    	else
    	{
    		//bad id
    		GENFSK_AbortAll();
    		GENFSK_StartRx(mAppGenfskId, gRxBuffer, gGenFskDefaultMaxBufferSize_c+crcConfig.crcSize, 0, 0);
    	}
#endif
    }
    else if(flags & gCtEvtTxDone_c)
    {
    	Serial_Print(mAppSerId,"Finished transmission\r\n",gAllowToBlock_d);
    	GENFSK_AbortAll();
    	GENFSK_StartRx(mAppGenfskId, gRxBuffer, gGenFskDefaultMaxBufferSize_c+crcConfig.crcSize, 0, 0);
    }
    else if(flags & gCtEvtTimerExpired_c)
    {
#ifdef LEDCONTROL_MASTER
		if(conState == gAppSlave1Check)
		{
			if(!slave1connected)
			{
				Serial_Print(mAppSerId,"Slave 1 disconnected\r\n",gAllowToBlock_d);
			}
			else
			{
				Serial_Print(mAppSerId,"Slave 1 connected\r\n",gAllowToBlock_d);
			}
			conState = gAppSlave2Check;
			slave2connected = false;
			gTxPacket.payload[0] = LEDCONTROL_DEVICE_ID_ONE;
		}
		else if(conState == gAppSlave2Check)
		{
			if(!slave2connected)
			{
				Serial_Print(mAppSerId,"Slave 2 disconnected\r\n",gAllowToBlock_d);
			}
			else
			{
				Serial_Print(mAppSerId,"Slave 2 connected\r\n",gAllowToBlock_d);
			}
			conState = gAppSlave3Check;
			slave3connected = false;
			gTxPacket.payload[0] = LEDCONTROL_DEVICE_ID_TWO;
		}
		else if(conState == gAppSlave3Check)
		{
			if(!slave3connected)
			{
				Serial_Print(mAppSerId,"Slave 3 disconnected\r\n",gAllowToBlock_d);
			}
			else
			{
				Serial_Print(mAppSerId,"Slave 3 connected\r\n",gAllowToBlock_d);
			}
			conState = gAppSlave1Check;
			slave1connected = false;
			gTxPacket.payload[0] = LEDCONTROL_DEVICE_ID_ZERO;
		}
		else
		{
			//unreachable
		}
		gTxPacket.payload[1] = 'v';
		buffLen = gTxPacket.header.lengthField+(gGenFskDefaultHeaderSizeBytes_c)+(gGenFskDefaultSyncAddrSize_c + 1);
		GENFSK_PacketToByteArray(mAppGenfskId, &gTxPacket, gTxBuffer);
		GENFSK_AbortAll();
		GENFSK_StartTx(mAppGenfskId, gTxBuffer, buffLen, 0);
#endif

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
	gTxPacket.header.lengthField = gGenFskMinPayloadLen_c;
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



