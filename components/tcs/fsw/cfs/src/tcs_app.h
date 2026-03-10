/*******************************************************************************
** File: tcs_app.h
**
** Purpose:
**   This is the main header file for the TCS application.
**
*******************************************************************************/
#ifndef _TCS_APP_H_
#define _TCS_APP_H_

/*
** Include Files
*/
#include "cfe.h"
#include "tcs_device.h"
#include "tcs_events.h"
#include "tcs_platform_cfg.h"
#include "tcs_perfids.h"
#include "tcs_msg.h"
#include "tcs_msgids.h"
#include "tcs_version.h"
#include "hwlib.h"

/* TODO: This is specific to the tcs application, remove if using template generator */
#include "mgr_msg.h"
#include "mgr_msgids.h"

/*
** Specified pipe depth - how many messages will be queued in the pipe
*/
#define TCS_PIPE_DEPTH 32

/*
** Enabled and Disabled Definitions
*/
#define TCS_DEVICE_DISABLED 0
#define TCS_DEVICE_ENABLED  1

/*
** TCS global data structure
** The cFE convention is to put all global app data in a single struct.
** This struct is defined in the `tcs_app.h` file with one global instance
** in the `.c` file.
*/
typedef struct
{
    /*
    ** Housekeeping telemetry packet
    ** Each app defines its own packet which contains its OWN telemetry
    */
    TCS_Hk_tlm_t HkTelemetryPkt; /* TCS Housekeeping Telemetry Packet */

    /*
    ** Operational data  - not reported in housekeeping
    */
    CFE_MSG_Message_t *MsgPtr;    /* Pointer to msg received on software bus */
    CFE_SB_PipeId_t    CmdPipe;   /* Pipe Id for HK command pipe */
    uint32             RunStatus; /* App run status for controlling the application state */

    /*
     ** Device data
     ** TODO: Make specific to your application
     */
    TCS_Device_tlm_t DevicePkt; /* Device specific data packet */

    /*
    ** Device protocol
    ** TODO: Make specific to your application
    */
    uart_info_t TcsUart; /* Hardware protocol definition */

} TCS_AppData_t;

/*
** Exported Data
** Extern the global struct in the header for the Unit Test Framework (UTF).
*/
extern TCS_AppData_t TCS_AppData; /* TCS App Data */

/*
**
** Local function prototypes.
**
** Note: Except for the entry point (TCS_AppMain), these
**       functions are not called from any other source module.
*/
void  TCS_AppMain(void);
int32 TCS_AppInit(void);
void  TCS_ProcessCommandPacket(void);
void  TCS_ProcessGroundCommand(void);
void  TCS_ProcessTelemetryRequest(void);
void  TCS_ReportHousekeeping(void);
void  TCS_ReportDeviceTelemetry(void);
void  TCS_ResetCounters(void);
void  TCS_Enable(void);
void  TCS_Disable(void);
void  TCS_Configure(void);
int32 TCS_VerifyCmdLength(CFE_MSG_Message_t *msg, uint16 expected_length);

/* TODO: This is specific to the tcs application, remove if using template generator */
void TCS_ProcessMgrHk(void);

#endif /* _TCS_APP_H_ */
