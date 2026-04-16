/*******************************************************************************
** File:
**   tcs_msg.h
**
** Purpose:
**  Define TCS application commands and telemetry messages
**
** Implementation Notes:
**   OpenC3 command definitions, XTCE command definitions, unit tests, and
**   TCS_ProcessGroundCommand() all depend on the numeric command-code values
**   below.  Changing any value requires updating:
**     components/tcs/gsw/TCS/cmd_tlm/TCS_CMD.txt
**     components/tcs/gsw/tcs.xtce
**     components/tcs/fsw/cfs/src/tcs_app.c
**     components/tcs/fsw/cfs/unit-test/coveragetest/coveragetest_tcs_app.c
**
*******************************************************************************/
#ifndef _TCS_MSG_H_
#define _TCS_MSG_H_

#include "cfe.h"
#include "tcs_device.h"

/*
** Ground Command Codes
** The heater commands are no-argument cFS commands.  HEATER_ENABLE and
** HEATER_DISABLE force MANUAL mode before writing the heater state; HEATER_AUTO
** only writes AUTO mode and lets the simulator apply threshold hysteresis.
*/
#define TCS_NOOP_CC           0
#define TCS_RESET_COUNTERS_CC 1
#define TCS_HEATER_ENABLE_CC  2
#define TCS_HEATER_DISABLE_CC 3
#define TCS_CONFIG_CC         4
#define TCS_ENABLE_CC         5
#define TCS_DISABLE_CC        6
#define TCS_HEATER_AUTO_CC    7

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
    /*
    ** Tcs is the UART device payload decoded in tcs_device.c.  It contains the
    ** internal temperature, Kelvin thresholds, heater state, control mode, and
    ** skin temperature produced by the simulator's two-node thermal model.
    */
    TCS_Device_Data_tlm_t  Tcs;

    /* TODO: This is specific to the tcs application, remove if using template generator */
    /*
    ** These two fields are copied from MGR HK in TCS_ProcessMgrHk().  They are
    ** appended after the TCS device bytes so the packet still starts with the
    ** exact TCS_Device_Data_tlm_t layout expected by OpenC3/XTCE.
    */
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
    ** DeviceEnabled is app-side UART state: ENABLED means TCS_Enable opened the
    ** port successfully, not that the heater is on.  Heater state is reported
    ** inside DeviceHK/data telemetry from the simulator.
    */
    uint8                  DeviceEnabled;
    /*
    ** DeviceHK mirrors the 16-byte simulator housekeeping response after the
    ** 0xDEAD header and before the 0xBEEF trailer are stripped by tcs_device.c.
    */
    TCS_Device_HK_tlm_t DeviceHK;

} __attribute__((packed)) TCS_Hk_tlm_t;
#define TCS_HK_TLM_LNGTH sizeof(TCS_Hk_tlm_t)

#endif /* _TCS_MSG_H_ */
