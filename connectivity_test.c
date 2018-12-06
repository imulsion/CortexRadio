#include "LED.h"


#include "MemManager.h"
#include "TimersManager.h"
#include "Messaging.h"
#include "SecLib.h"
#include "Panic.h"
#include "fsl_xcvr.h"
#include "fsl_os_abstraction.h"
#include "SerialManager.h"

#include "board.h"

#include "FreeRTOSConfig.h"

#include "genfsk_interface.h"
#include "connectivity_test.h"

#define App_NotifySelf() OSA_EventSet(mAppThreadEvt, gCtEvtSelfEvent_c)

/*Application main*/
static void App_Thread (uint32_t param); 
/*Application event handler*/
static void App_HandleEvents(osaEventFlags_t flags);
/*Function that reads latest byte from Serial Manager*/
static void App_UpdateUartData(uint8_t* pData);
/*Application Init*/
static void App_InitApp();

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
/*Application Thread notification function (sends event to application task)*/
static void App_NotifyAppThread(void);
/*Timer callback*/
static void App_TimerCallback(void* param);



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


/*set TRUE when user presses [ENTER] on logo screen*/
static bool_t mAppStartApp = FALSE;

static bool_t mAppInit = false;


/*structure to store information regarding latest received packet*/
static ct_rx_indication_t mAppRxLatestPacket;

/*latest generic fsk event status*/
static genfskEventStatus_t mAppGenfskStatus;

/* GENFSK instance id*/
uint8_t mAppGenfskId;

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
        TMR_Init();
        //initialize Serial Manager
        SerialManager_Init();
        LED_Init();
        SecLib_Init();
        

        GENFSK_Init();
        
        /* GENFSK LL Init with default register config */
        GENFSK_AllocInstance(&mAppGenfskId, NULL, NULL, NULL);   
        
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
        
        /*allocate a timer*/
        mAppTmrId = TMR_AllocateTimer();
        Serial_Print(mAppSerId, "Hi welcome to this shitty led thing press enter to start\r\n", gAllowToBlock_d);
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
    
    while(1)
    {
        (void)OSA_EventWait(mAppThreadEvt, gCtEvtEventsAll_c, FALSE, osaWaitForever_c ,&mAppThreadEvtFlags);
        if(mAppThreadEvtFlags)
        {
            if(mAppStartApp)
            {
                App_HandleEvents(mAppThreadEvtFlags);/*handle app events*/
            }
            else
            {
                if(mAppThreadEvtFlags & gCtEvtUart_c) /*if uart event*/
                {
                    App_UpdateUartData(&mAppUartData); /*read new byte*/
                    if(mAppUartData == '\r')
                    {
                        mAppStartApp = TRUE;
                        /*notify task again to start running*/
                        App_NotifySelf();
                    }
                    else
                    {
                        /*if other key is pressed show screen again*/
                        Serial_Print(mAppSerId, "press enter dickhead dont make me repeat myself\r\n", gAllowToBlock_d);
                    }
                }
            }
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
    }
    if((mAppUartData == '\r') && (!mAppInit))
    {
    	Serial_Print(mAppSerId, "well done you pressed enter you genius have fun playing with LEDs\r\n",gAllowToBlock_d);
    	mAppInit = true;
    }
    else
    {
		if((mAppUartData != 'r')&&(mAppUartData != 'g')&&(mAppUartData != 'b'))
		{
			Serial_Print(mAppSerId, "wrong button dumbass try again\r\n", gAllowToBlock_d);
		}
		else
		{
			if(mAppUartData == 'r')
			{
				Led2Toggle();
			}
			else if(mAppUartData == 'g')
			{
				Led3Toggle();
			}
			else
			{
				Led4Toggle();
			}
		}
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
* \brief  Application initialization. It installs the main menu callbacks and
*         calls the Connectivity Test for Generic FSK init.
*
********************************************************************************** */
static void App_InitApp()
{   

   /*register callbacks for the generic fsk LL */
   GENFSK_RegisterCallbacks(mAppGenfskId,
                            App_GenFskReceiveCallback, 
                            App_GenFskEventNotificationCallback);
   
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

static void App_NotifyAppThread(void)
{
    App_NotifySelf();
}
static void App_TimerCallback(void* param)
{
    OSA_EventSet(mAppThreadEvt, gCtEvtTimerExpired_c);
}



