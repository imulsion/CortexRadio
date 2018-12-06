
#ifndef _APPL_MAIN_H_
#define _APPL_MAIN_H_


/*! *********************************************************************************
*************************************************************************************
* Include
*************************************************************************************
********************************************************************************** */
#include "EmbeddedTypes.h"
#include "fsl_os_abstraction.h"

/*! *********************************************************************************
*************************************************************************************
* Public type definitions
*************************************************************************************
********************************************************************************** */
/*application state machine states enumeration*/
typedef enum app_states_tag
{
    gAppStateInit_c = 0,
    gAppStateIdle_c,
    gAppStateSelectTest_c,
    gAppStateRunning_c,
    gAppStateMaxState_c
}app_states_t;

/*! *********************************************************************************
*************************************************************************************
* Public macros
*************************************************************************************
********************************************************************************** */
/*
 * These values should be modified by the application as necessary.
 * They are used by the idle task initialization code from ApplMain.c.
 */
/*! Idle Task Stack Size */
#ifndef gAppIdleTaskStackSize_c
#define gAppIdleTaskStackSize_c (400)
#endif

/*! Idle Task OS Abstraction Priority */
#ifndef gAppIdleTaskPriority_c
#define gAppIdleTaskPriority_c  (8)
#endif


typedef enum ct_event_tag
{
  gCtEvtRxDone_c       = 0x00000001U,
  gCtEvtTxDone_c       = 0x00000002U,
  gCtEvtSeqTimeout_c   = 0x00000004U,
  gCtEvtRxFailed_c     = 0x00000008U,

  gCtEvtTimerExpired_c = 0x00000010U,
  gCtEvtUart_c         = 0x00000020U,
  gCtEvtKBD_c          = 0x00000040U,
  gCtEvtSelfEvent_c    = 0x00000080U,

  gCtEvtWakeUp_c       = 0x00000100U,

  gCtEvtMaxEvent_c     = 0x00000200U,
  gCtEvtEventsAll_c    = 0x000003FFU
}ct_event_t;

typedef enum ct_param_type_tag{
    gParamTypeNumber_c = 0,
    gParamTypeString_c,
    gParamTypeBool_c,
    gParamTypeMaxType_c
}ct_param_type_t;

typedef struct ct_config_params_tag
{
    ct_param_type_t paramType;
    uint8_t paramName[20];
    union
    {
        uint32_t decValue;
        uint8_t stringValue[4];
        bool_t  boolValue;
    }
    paramValue;
}ct_config_params_t;

typedef struct ct_rx_indication_tag
{
    uint64_t timestamp;
    uint8_t *pBuffer;
    uint16_t bufferLength;
    uint8_t rssi;
    uint8_t crcValid;
}ct_rx_indication_t;

typedef void (* pHookAppNotification) ( void );
typedef void (* pTmrHookNotification) (void*);
/*! *********************************************************************************
*************************************************************************************
* Public macros
*************************************************************************************
********************************************************************************** */
#define gModeRx_c (1)
#define gModeTx_c (2)
#define gDefaultMode_c gModeRx_c

/*tx power*/
#define gGenFskMaxTxPowerLevel_c     (0x20)
#define gGenFskMinTxPowerLevel_c     (0x00)
#define gGenFskDefaultTxPowerLevel_c (0x08)

/*channel*/
#define gGenFskMaxChannel_c     (0x7F)
#define gGenFskMinChannel_c     (0x00)
#define gGenFskDefaultChannel_c (0x2A)

/*network address*/
#define gGenFskDefaultSyncAddress_c  (0x8E89BED6)
#define gGenFskDefaultSyncAddrSize_c (0x03) /*bytes = size + 1*/

/*the following field sizes must be multiple of 8 bit*/
#define gGenFskDefaultH0FieldSize_c     (8)
#define gGenFskDefaultLengthFieldSize_c (6)
#define gGenFskDefaultH1FieldSize_c     (2)
#define gGenFskDefaultHeaderSizeBytes_c ((gGenFskDefaultH0FieldSize_c + \
                                         gGenFskDefaultLengthFieldSize_c + \
                                             gGenFskDefaultH1FieldSize_c) >> 3)

/*payload length*/
#define gGenFskMaxPayloadLen_c ((1 << gGenFskDefaultLengthFieldSize_c) - 1)

/*test opcode + 2byte packet index + 2byte number of packets for PER test*/

#define gGenFskMinPayloadLen_c (6)

#define gGenFskDefaultPayloadLen_c (gGenFskMinPayloadLen_c)

#define gGenFskDefaultMaxBufferSize_c (gGenFskDefaultSyncAddrSize_c + 1 + \
                                       gGenFskDefaultHeaderSizeBytes_c  + \
                                           gGenFskMaxPayloadLen_c)

/*H0 and H1 config*/
#define gGenFskDefaultH0Value_c        (0x0000)
#define gGenFskDefaultH0Mask_c         ((1 << gGenFskDefaultH0FieldSize_c) - 1)

#define gGenFskDefaultH1Value_c        (0x0000)
#define gGenFskDefaultH1Mask_c         ((1 << gGenFskDefaultH1FieldSize_c) - 1)
/*! *********************************************************************************
*************************************************************************************
* Public memory declarations
*************************************************************************************
********************************************************************************** */
ct_config_params_t gaConfigParams[];
uint8_t mAppSerId;
uint8_t mAppTmrId;


#endif /* _APPL_MAIN_H_ */
