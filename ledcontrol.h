#ifndef _APPL_MAIN_H_
#define _APPL_MAIN_H_


/*! *********************************************************************************
*************************************************************************************
* Include
*************************************************************************************
********************************************************************************** */
#include "EmbeddedTypes.h"
#include "fsl_os_abstraction.h"
#include "genfsk_interface.h"

/*! *********************************************************************************
*************************************************************************************
* Public type definitions
*************************************************************************************
********************************************************************************** */
/*application state machine states enumeration*/
typedef enum app_states_tag
{
	gAppTx_c = 0,
	gAppRx_c = 1,
}app_states_t;

typedef enum
{
	gAppSlave1Check = 0,
	gAppSlave2Check = 1,
	gAppSlave3Check = 2,
}connectivity_states_t;

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

/*LED packet type*/
typedef enum
{

	LEDControl_LED_RED_c = 0,
	LEDControl_LED_GREEN_c = 1,
	LEDControl_LED_BLUE_c = 2,
	LEDControl_LED_Verify_c = 3,

}LEDControl_LED_Data_t;

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
/*
 * These values should be modified by the application as necessary.
 */
/*! Idle Task Stack Size */
#ifndef gAppIdleTaskStackSize_c
#define gAppIdleTaskStackSize_c (400)
#endif

/*! Idle Task OS Abstraction Priority */
#ifndef gAppIdleTaskPriority_c
#define gAppIdleTaskPriority_c  (8)
#endif

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

#define LEDCONTROL_DEVICE_ID_ZERO 0
#define LEDCONTROL_DEVICE_ID_ONE 1
#define LEDCONTROL_DEVICE_ID_TWO 2

/*Master/Slave select*/
//#define LEDCONTROL_MASTER

/*Device ID*/
#ifndef LEDCONTROL_MASTER
#define LEDCONTROL_DEVICE_ID LEDCONTROL_DEVICE_ID_ZERO
#endif

#ifdef LEDCONTROL_MASTER
#define LEDCONTROL_CONNECTIONCHECK_TIMEOUT_MILLISECONDS 500 // number of milliseconds without reply before slave is considered disconnected
#endif


/*! *********************************************************************************
*************************************************************************************
* Public memory declarations
*************************************************************************************
********************************************************************************** */

/*packet configuration*/
static GENFSK_packet_config_t pktConfig =
{
    .preambleSizeBytes = 0, /*1 byte of preamble*/
    .packetType = gGenfskFormattedPacket,
    .lengthSizeBits = gGenFskDefaultLengthFieldSize_c,
    .lengthBitOrder = gGenfskLengthBitLsbFirst,
    /*sync address bytes = size + 1*/
    .syncAddrSizeBytes = gGenFskDefaultSyncAddrSize_c,
    .lengthAdjBytes = 3, /*length field not including CRC so adjust by crc len*/
    .h0SizeBits = gGenFskDefaultH0FieldSize_c,
    .h1SizeBits = gGenFskDefaultH1FieldSize_c,
    .h0Match = gGenFskDefaultH0Value_c, /*match field containing zeros*/
    .h0Mask = gGenFskDefaultH0Mask_c,
    .h1Match = gGenFskDefaultH1Value_c,
    .h1Mask = gGenFskDefaultH1Mask_c
};

/*CRC configuration*/
static GENFSK_crc_config_t crcConfig =
{
    .crcEnable = gGenfskCrcEnable,
    .crcSize = 3,
    .crcStartByte = 4,
    .crcRefIn = gGenfskCrcInputNoRef,
    .crcRefOut = gGenfskCrcOutputNoRef,
    .crcByteOrder = gGenfskCrcLSByteFirst,
    .crcSeed = 0x00555555,
    .crcPoly = 0x0000065B,
    .crcXorOut = 0
};

/*whitener configuration*/
static GENFSK_whitener_config_t whitenConfig =
{
    .whitenEnable = gGenfskWhitenEnable,
    .whitenStart = gWhitenStartWhiteningAtH0,
    .whitenEnd = gWhitenEndAtEndOfCrc,
    .whitenB4Crc = gCrcB4Whiten,
    .whitenPolyType = gGaloisPolyType,
    .whitenRefIn = gGenfskWhitenInputNoRef,
    .whitenPayloadReinit = gGenfskWhitenNoPayloadReinit,
    .whitenSize = 7,
    .whitenInit = 0x53,
    .whitenPoly = 0x11, /*x^7 + x^4 + 1! x^7 is never set*/
    .whitenSizeThr = 0,
    .manchesterEn = gGenfskManchesterDisable,
    .manchesterStart = gGenfskManchesterStartAtPayload,
    .manchesterInv = gGenfskManchesterNoInv,
};

/*radio configuration*/
static GENFSK_radio_config_t radioConfig =
{
    .radioMode = gGenfskGfskBt0p5h0p5,
    .dataRate = gGenfskDR1Mbps
};

/*network / sync address configuration*/
static GENFSK_nwk_addr_match_t ntwkAddr =
{
    .nwkAddrSizeBytes = gGenFskDefaultSyncAddrSize_c,
    .nwkAddrThrBits = 0,
    .nwkAddr = gGenFskDefaultSyncAddress_c,
};

/* GENFSK instance id*/
uint8_t mAppGenfskId;

/* Serial instance id*/
uint8_t mAppSerId;

/* Timer instance ID */
uint8_t mAppTmrId;

bool slave1connected,slave2connected,slave3connected;

#endif /* _APPL_MAIN_H_ */
