/*******************************************************************************
** File:
**   tcs_msg.h
**
** Purpose:
**  Define TCS application commands and telemetry messages
**
*******************************************************************************/
#ifndef _TCS_MSG_H_
#define _TCS_MSG_H_

#include "cfe.h"
#include "tcs_device.h"

/*
** Ground Command Codes
** TODO: Add additional commands required by the specific component
*/
#define TCS_NOOP_CC           0
#define TCS_RESET_COUNTERS_CC 1
#define TCS_ENABLE_CC         2
#define TCS_DISABLE_CC        3
#define TCS_CONFIG_CC         4

/*
** Telemetry Request Command Codes
** TODO: Add additional commands required by the specific component
*/
#define TCS_REQ_HK_TLM   0
#define TCS_REQ_DATA_TLM 1

/*
** Generic "no arguments" command type definition
*/
typedef struct
{
    /* Every command requires a header used to identify it */
    CFE_MSG_CommandHeader_t CmdHeader;

} TCS_NoArgs_cmd_t;

/*
** TCS write configuration command
*/
typedef struct
{
    CFE_MSG_CommandHeader_t CmdHeader;
    uint32                  DeviceCfg;

} TCS_Config_cmd_t;

/*
** TCS device telemetry definition
*/
typedef struct
{
    CFE_MSG_TelemetryHeader_t TlmHeader;
    TCS_Device_Data_tlm_t  Tcs;

    /* TODO: This is specific to the tcs application, remove if using template generator */
    uint16 PassNumber;
    uint8  RegionStatus;

} __attribute__((packed)) TCS_Device_tlm_t;
#define TCS_DEVICE_TLM_LNGTH sizeof(TCS_Device_tlm_t)

/*
** TCS housekeeping type definition
*/
typedef struct
{
    CFE_MSG_TelemetryHeader_t TlmHeader;
    uint8                     CommandErrorCount;
    uint8                     CommandCount;
    uint8                     DeviceErrorCount;
    uint8                     DeviceCount;

    /*
    ** TODO: Edit and add specific telemetry values to this struct
    */
    uint8                  DeviceEnabled;
    TCS_Device_HK_tlm_t DeviceHK;

} __attribute__((packed)) TCS_Hk_tlm_t;
#define TCS_HK_TLM_LNGTH sizeof(TCS_Hk_tlm_t)

#endif /* _TCS_MSG_H_ */
