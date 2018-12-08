#include "LED.h"
#include "ledcontrol.h"

#include "MemManager.h"
#include "Messaging.h"
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


static app_states_t appState = gAppRx_c;

/*structure to store information regarding latest received packet*/
static ct_rx_indication_t mAppRxLatestPacket;

/*latest generic fsk event status*/
static genfskEventStatus_t mAppGenfskStatus;

/* GENFSK instance id*/
uint8_t mAppGenfskId;

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

        Serial_Print(mAppSerId, "Welcome to the remote LED controller.\r\n", gAllowToBlock_d);
		Serial_Print(mAppSerId,"Press the [r], [g] and [b] keys to control LEDs\r\n",gAllowToBlock_d);

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

    while(1)
    {
    	switch(appState)
    	{
    	case gAppRx_c:
    		GENFSK_AbortAll();
    		GENFSK_StartRx(mAppGenfskId, gRxBuffer, gGenFskDefaultMaxBufferSize_c+crcConfig.crcSize, 0, 0);
    		break;
    	case gAppTx_c:
    		Serial_Print(mAppSerId,"Starting transmission...\r\n",gAllowToBlock_d);
    		GENFSK_AbortAll();
    		GENFSK_StartTx(mAppGenfskId,gTxBuffer,buffLen,0);
    		break;
    	default: break;


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
        App_UpdateUartData(&mAppUartData);

		if((mAppUartData != 'r')&&(mAppUartData != 'g')&&(mAppUartData != 'b'))
		{
			appState = gAppRx_c;
		}
		else
		{
			gTxPacket.header.lengthField = gGenFskMinPayloadLen_c;
			if(mAppUartData == 'r')
			{
				gTxPacket.payload[0] = 0;
				buffLen = gTxPacket.header.lengthField+(gGenFskDefaultHeaderSizeBytes_c)+(gGenFskDefaultSyncAddrSize_c + 1);
				GENFSK_PacketToByteArray(mAppGenfskId, &gTxPacket, gTxBuffer);
				appState = gAppTx_c;
			}
			else if(mAppUartData == 'g')
			{
				gTxPacket.payload[0] = 1;
				buffLen = gTxPacket.header.lengthField+(gGenFskDefaultHeaderSizeBytes_c)+(gGenFskDefaultSyncAddrSize_c + 1);
				GENFSK_PacketToByteArray(mAppGenfskId, &gTxPacket, gTxBuffer);
				appState = gAppTx_c;
			}
			else
			{
				gTxPacket.payload[0] = 2;
				buffLen = gTxPacket.header.lengthField+(gGenFskDefaultHeaderSizeBytes_c)+(gGenFskDefaultSyncAddrSize_c + 1);
				GENFSK_PacketToByteArray(mAppGenfskId, &gTxPacket, gTxBuffer);
				appState = gAppTx_c;
			}
		}
    }
    else if(flags & gCtEvtRxDone_c) //received comms
    {

    	GENFSK_ByteArrayToPacket(mAppGenfskId, mAppRxLatestPacket.pBuffer, &gRxPacket);
    	uint8_t data = gRxPacket.payload[0];
    	if(data == 0)
    	{
    		Led2Toggle();
    	}
    	else if(data == 1)
    	{
    		Led3Toggle();
    	}
    	else if(data == 2)
    	{
    		Led4Toggle();
    	}
    	else
    	{
    	}

    	appState = gAppRx_c;
    }
    else if(flags & gCtEvtTxDone_c)
    {
    	Serial_Print(mAppSerId,"Transmission finished\r\n",gAllowToBlock_d);
    	appState = gAppRx_c;
    }
    else{}
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



