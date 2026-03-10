/*******************************************************************************
** File: tcs_app.c
**
** Purpose:
**   This file contains the source code for the TCS application.
**
*******************************************************************************/

/*
** Include Files
*/
#include <arpa/inet.h>
#include "tcs_app.h"

/*
** Global Data
*/
TCS_AppData_t TCS_AppData;

/*
** Application entry point and main process loop
*/
void TCS_AppMain(void)
{
    int32 status = OS_SUCCESS;

    /*
    ** Create the first Performance Log entry
    */
    CFE_ES_PerfLogEntry(TCS_PERF_ID);

    /*
    ** Perform application initialization
    */
    status = TCS_AppInit();
    if (status != CFE_SUCCESS)
    {
        TCS_AppData.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    /*
    ** Main loop
    */
    while (CFE_ES_RunLoop(&TCS_AppData.RunStatus) == true)
    {
        /*
        ** Performance log exit stamp
        */
        CFE_ES_PerfLogExit(TCS_PERF_ID);

        /*
        ** Pend on the arrival of the next Software Bus message
        ** Note that this is the standard, but timeouts are available
        */
        status = CFE_SB_ReceiveBuffer((CFE_SB_Buffer_t **)&TCS_AppData.MsgPtr, TCS_AppData.CmdPipe,
                                      CFE_SB_PEND_FOREVER);

        /*
        ** Begin performance metrics on anything after this line. This will help to determine
        ** where we are spending most of the time during this app execution.
        */
        CFE_ES_PerfLogEntry(TCS_PERF_ID);

        /*
        ** If the CFE_SB_ReceiveBuffer was successful, then continue to process the command packet
        ** If not, then exit the application in error.
        ** Note that a SB read error should not always result in an app quitting.
        */
        if (status == CFE_SUCCESS)
        {
            TCS_ProcessCommandPacket();
        }
        else
        {
            CFE_EVS_SendEvent(TCS_PIPE_ERR_EID, CFE_EVS_EventType_ERROR, "TCS: SB Pipe Read Error = %d",
                              (int)status);
            TCS_AppData.RunStatus = CFE_ES_RunStatus_APP_ERROR;
        }
    }

    /*
    ** Disable component, which cleans up the interface, upon exit
    */
    TCS_Disable();

    /*
    ** Performance log exit stamp
    */
    CFE_ES_PerfLogExit(TCS_PERF_ID);

    /*
    ** Exit the application
    */
    CFE_ES_ExitApp(TCS_AppData.RunStatus);
}

/*
** Initialize application
*/
int32 TCS_AppInit(void)
{
    int32 status = OS_SUCCESS;

    TCS_AppData.RunStatus = CFE_ES_RunStatus_APP_RUN;

    /*
    ** Register the events
    */
    status = CFE_EVS_Register(NULL, 0, CFE_EVS_EventFilter_BINARY); /* as default, no filters are used */
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("TCS: Error registering for event services: 0x%08X\n", (unsigned int)status);
        return status;
    }

    /*
    ** Create the Software Bus command pipe
    */
    status = CFE_SB_CreatePipe(&TCS_AppData.CmdPipe, TCS_PIPE_DEPTH, "TCS_CMD_PIPE");
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(TCS_PIPE_ERR_EID, CFE_EVS_EventType_ERROR, "Error Creating SB Pipe,RC=0x%08X",
                          (unsigned int)status);
        return status;
    }

    /*
    ** Subscribe to ground commands
    */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(TCS_CMD_MID), TCS_AppData.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(TCS_SUB_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Error Subscribing to HK Gnd Cmds, MID=0x%04X, RC=0x%08X", TCS_CMD_MID,
                          (unsigned int)status);
        return status;
    }

    /*
    ** Subscribe to housekeeping (hk) message requests
    */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(TCS_REQ_HK_MID), TCS_AppData.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(TCS_SUB_REQ_HK_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Error Subscribing to HK Request, MID=0x%04X, RC=0x%08X", TCS_REQ_HK_MID,
                          (unsigned int)status);
        return status;
    }

    /*
    ** Subscribe to MGR HK for Science Pass Information
    ** TODO: This is specific to the tcs application, remove if using template generator
    */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(MGR_HK_TLM_MID), TCS_AppData.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(TCS_SUB_REQ_HK_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Error Subscribing to HK Request, MID=0x%04X, RC=0x%08X", MGR_HK_TLM_MID,
                          (unsigned int)status);
        return status;
    }

    /*
    ** TODO: Subscribe to any other messages here
    */

    /*
    ** Initialize the published HK message - this HK message will contain the
    ** telemetry that has been defined in the TCS_HkTelemetryPkt for this app.
    */
    CFE_MSG_Init(CFE_MSG_PTR(TCS_AppData.HkTelemetryPkt.TlmHeader), CFE_SB_ValueToMsgId(TCS_HK_TLM_MID),
                 TCS_HK_TLM_LNGTH);

    /*
    ** Initialize the device packet message
    ** This packet is specific to your application
    */
    CFE_MSG_Init(CFE_MSG_PTR(TCS_AppData.DevicePkt.TlmHeader), CFE_SB_ValueToMsgId(TCS_DEVICE_TLM_MID),
                 TCS_DEVICE_TLM_LNGTH);

    /*
    ** TODO: Initialize any other messages that this app will publish
    */

    /*
    ** Always reset all counters during application initialization
    */
    TCS_ResetCounters();

    /*
    ** Initialize application data
    ** Note that counters are excluded as they were reset in the previous code block
    */
    TCS_AppData.HkTelemetryPkt.DeviceEnabled          = TCS_DEVICE_DISABLED;
    TCS_AppData.HkTelemetryPkt.DeviceHK.DeviceCounter = 0;
    TCS_AppData.HkTelemetryPkt.DeviceHK.DeviceConfig  = 0;
    TCS_AppData.HkTelemetryPkt.DeviceHK.DeviceStatus  = 0;

    /*
     ** Send an information event that the app has initialized.
     ** This is useful for debugging the loading of individual applications.
     */
    status = CFE_EVS_SendEvent(TCS_STARTUP_INF_EID, CFE_EVS_EventType_INFORMATION,
                               "TCS App Initialized. Version %d.%d.%d.%d", TCS_MAJOR_VERSION,
                               TCS_MINOR_VERSION, TCS_REVISION, TCS_MISSION_REV);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("TCS: Error sending initialization event: 0x%08X\n", (unsigned int)status);
    }
    return status;
}

/*
** Process packets received on the TCS command pipe
*/
void TCS_ProcessCommandPacket(void)
{
    CFE_SB_MsgId_t MsgId = CFE_SB_INVALID_MSG_ID;
    CFE_MSG_GetMsgId(TCS_AppData.MsgPtr, &MsgId);
    switch (CFE_SB_MsgIdToValue(MsgId))
    {
        /*
        ** Ground Commands with command codes fall under the TCS_CMD_MID (Message ID)
        */
        case TCS_CMD_MID:
            TCS_ProcessGroundCommand();
            break;

        /*
        ** Housekeeping requests with command codes fall under the TCS_REQ_HK_MID (Message ID)
        */
        case TCS_REQ_HK_MID:
            TCS_ProcessTelemetryRequest();
            break;

        /*
        ** Update science pass information
        ** TODO: This is specific to the tcs application, remove if using template generator
        */
        case MGR_HK_TLM_MID:
            TCS_ProcessMgrHk();
            break;

        /*
        ** TODO: Add additional message IDs as needed
        */

        /*
        ** All other invalid messages that this app doesn't recognize,
        ** increment the command error counter and log as an error event.
        */
        default:
            /* Increment the command error counter upon receipt of an invalid command packet */
            TCS_AppData.HkTelemetryPkt.CommandErrorCount++;

            /* Send event failure to the console*/
            CFE_EVS_SendEvent(TCS_PROCESS_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "TCS: Invalid command packet, MID = 0x%x", CFE_SB_MsgIdToValue(MsgId));
            break;
    }
    return;
}

/*
** Process ground commands
** TODO: Add additional commands required by the specific component
*/
void TCS_ProcessGroundCommand(void)
{
    CFE_SB_MsgId_t    MsgId       = CFE_SB_INVALID_MSG_ID;
    CFE_MSG_FcnCode_t CommandCode = 0;

    /*
    ** MsgId is only needed if the command code is not recognized. See default case
    */
    CFE_MSG_GetMsgId(TCS_AppData.MsgPtr, &MsgId);

    /*
    ** Ground Commands have a command code (_CC) associated with them
    ** Pull this command code from the message and then process
    */
    CFE_MSG_GetFcnCode(TCS_AppData.MsgPtr, &CommandCode);
    switch (CommandCode)
    {
        /*
        ** NOOP Command
        */
        case TCS_NOOP_CC:
            /*
            ** Verify the command length immediately after CC identification
            */
            if (TCS_VerifyCmdLength(TCS_AppData.MsgPtr, sizeof(TCS_NoArgs_cmd_t)) == OS_SUCCESS)
            {
#ifdef TCS_CFG_DEBUG
                OS_printf("TCS: TCS_NOOP_CC received \n");
#endif

                /* Do any necessary checks, none for a NOOP */

                /* Increment command success or error counter, NOOP can only be successful */
                TCS_AppData.HkTelemetryPkt.CommandCount++;

                /* Do the action, none for a NOOP */

                /* Increment device success or error counter, none for NOOP as application only */

                /* Send event success or failure to the console, NOOP can only be successful */
                CFE_EVS_SendEvent(TCS_CMD_NOOP_INF_EID, CFE_EVS_EventType_INFORMATION,
                                  "TCS: NOOP command received");
            }
            break;

        /*
        ** Reset Counters Command
        */
        case TCS_RESET_COUNTERS_CC:
            if (TCS_VerifyCmdLength(TCS_AppData.MsgPtr, sizeof(TCS_NoArgs_cmd_t)) == OS_SUCCESS)
            {
#ifdef TCS_CFG_DEBUG
                OS_printf("TCS: TCS_RESET_COUNTERS_CC received \n");
#endif
                TCS_ResetCounters();
            }
            break;

        /*
        ** Enable Command
        */
        case TCS_ENABLE_CC:
            if (TCS_VerifyCmdLength(TCS_AppData.MsgPtr, sizeof(TCS_NoArgs_cmd_t)) == OS_SUCCESS)
            {
#ifdef TCS_CFG_DEBUG
                OS_printf("TCS: TCS_ENABLE_CC received \n");
#endif
                TCS_Enable();
            }
            break;

        /*
        ** Disable Command
        */
        case TCS_DISABLE_CC:
            if (TCS_VerifyCmdLength(TCS_AppData.MsgPtr, sizeof(TCS_NoArgs_cmd_t)) == OS_SUCCESS)
            {
#ifdef TCS_CFG_DEBUG
                OS_printf("TCS: TCS_DISABLE_CC received \n");
#endif
                TCS_Disable();
            }
            break;

        /*
        ** Set Configuration Command
        ** Note that this is an example of a command that has additional arguments
        */
        case TCS_CONFIG_CC:
            if (TCS_VerifyCmdLength(TCS_AppData.MsgPtr, sizeof(TCS_Config_cmd_t)) == OS_SUCCESS)
            {
#ifdef TCS_CFG_DEBUG
                OS_printf("TCS: TCS_CONFIG_CC received \n");
#endif
                TCS_Configure();
            }
            break;

        /*
        ** TODO: Edit and add more command codes as appropriate for the application
        */

        /*
        ** Invalid Command Codes
        */
        default:
            /* Increment the command error counter upon receipt of an invalid command */
            TCS_AppData.HkTelemetryPkt.CommandErrorCount++;

            /* Send invalid command code failure to the console */
            CFE_EVS_SendEvent(TCS_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "TCS: Invalid command code for packet, MID = 0x%x, cmdCode = 0x%x",
                              CFE_SB_MsgIdToValue(MsgId), CommandCode);
            break;
    }
    return;
}

/*
** Process Telemetry Request - Triggered in response to a telemetry request
*/
void TCS_ProcessTelemetryRequest(void)
{
    CFE_SB_MsgId_t    MsgId       = CFE_SB_INVALID_MSG_ID;
    CFE_MSG_FcnCode_t CommandCode = 0;

    /* MsgId is only needed if the command code is not recognized. See default case */
    CFE_MSG_GetMsgId(TCS_AppData.MsgPtr, &MsgId);

    /* Pull this command code from the message and then process */
    CFE_MSG_GetFcnCode(TCS_AppData.MsgPtr, &CommandCode);
    switch (CommandCode)
    {
        case TCS_REQ_HK_TLM:
            TCS_ReportHousekeeping();
            break;

        case TCS_REQ_DATA_TLM:
            TCS_ReportDeviceTelemetry();
            break;

        /*
        ** TODO: Edit, add, or remove telemetry request codes appropriate for the application
        */

        /*
        ** Invalid Command Codes
        */
        default:
            /* Increment the error counter upon receipt of an invalid command */
            TCS_AppData.HkTelemetryPkt.CommandErrorCount++;

            /* Send invalid command code failure to the console */
            CFE_EVS_SendEvent(TCS_DEVICE_TLM_ERR_EID, CFE_EVS_EventType_ERROR,
                              "TCS: Invalid command code for packet, MID = 0x%x, cmdCode = 0x%x",
                              CFE_SB_MsgIdToValue(MsgId), CommandCode);
            break;
    }
    return;
}

/*
** Report Application Housekeeping
*/
void TCS_ReportHousekeeping(void)
{
    int32 status = OS_SUCCESS;

    /* Check that device is enabled */
    if (TCS_AppData.HkTelemetryPkt.DeviceEnabled == TCS_DEVICE_ENABLED)
    {
        status = TCS_RequestHK(&TCS_AppData.TcsUart,
                                  (TCS_Device_HK_tlm_t *)&TCS_AppData.HkTelemetryPkt.DeviceHK);
        if (status == OS_SUCCESS)
        {
            TCS_AppData.HkTelemetryPkt.DeviceCount++;
        }
        else
        {
            TCS_AppData.HkTelemetryPkt.DeviceErrorCount++;
            CFE_EVS_SendEvent(TCS_REQ_HK_ERR_EID, CFE_EVS_EventType_ERROR,
                              "TCS: Request device HK reported error %d", status);
        }
    }
    /* Intentionally do not report errors if disabled */

    /* Time stamp and publish housekeeping telemetry */
    CFE_SB_TimeStampMsg((CFE_MSG_Message_t *)&TCS_AppData.HkTelemetryPkt);
    CFE_SB_TransmitMsg((CFE_MSG_Message_t *)&TCS_AppData.HkTelemetryPkt, true);
    return;
}

/*
** Collect and Report Device Telemetry
*/
void TCS_ReportDeviceTelemetry(void)
{
    int32 status = OS_SUCCESS;

    /* Check that device is enabled */
    if (TCS_AppData.HkTelemetryPkt.DeviceEnabled == TCS_DEVICE_ENABLED)
    {
        status = TCS_RequestData(&TCS_AppData.TcsUart,
                                    (TCS_Device_Data_tlm_t *)&TCS_AppData.DevicePkt.Tcs);
        if (status == OS_SUCCESS)
        {
            /* Update packet count */
            TCS_AppData.HkTelemetryPkt.DeviceCount++;

            /* Time stamp and publish data telemetry */
            CFE_SB_TimeStampMsg((CFE_MSG_Message_t *)&TCS_AppData.DevicePkt);
            CFE_SB_TransmitMsg((CFE_MSG_Message_t *)&TCS_AppData.DevicePkt, true);
        }
        else
        {
            TCS_AppData.HkTelemetryPkt.DeviceErrorCount++;
            CFE_EVS_SendEvent(TCS_REQ_DATA_ERR_EID, CFE_EVS_EventType_ERROR,
                              "TCS: Request device data reported error %d", status);
        }

        /* Check device status and act on error */
        if (TCS_AppData.HkTelemetryPkt.DeviceHK.DeviceStatus != 0)
        {
            /* Any bit is an error, halting communication until device power cycled */
            TCS_Disable();

            /* Send device status error to the console */
            CFE_EVS_SendEvent(TCS_REQ_DATA_STATUS_ERR_EID, CFE_EVS_EventType_ERROR,
                              "TCS: Request device data reported status error %d",
                              TCS_AppData.HkTelemetryPkt.DeviceHK.DeviceStatus);
        }
    }
    /* Intentionally do not report errors if device disabled */
    return;
}

/*
** Ingest science MGR data and save it
** TODO: This is specific to the tcs application, remove if using template generator
*/
void TCS_ProcessMgrHk(void)
{
    MGR_Hk_tlm_t *pMsg = (MGR_Hk_tlm_t *)TCS_AppData.MsgPtr;

    TCS_AppData.DevicePkt.PassNumber   = pMsg->SciPassCount;
    TCS_AppData.DevicePkt.RegionStatus = pMsg->ScienceStatus;
    return;
}

/*
** Reset all global counter variables
*/
void TCS_ResetCounters(void)
{
    /* Do any necessary checks, none for reset counters */

    /* Increment command success or error counter, omitted as action is to reset */

    /* Do the action, clear all global counter variables */
    TCS_AppData.HkTelemetryPkt.CommandErrorCount = 0;
    TCS_AppData.HkTelemetryPkt.CommandCount      = 0;
    TCS_AppData.HkTelemetryPkt.DeviceErrorCount  = 0;
    TCS_AppData.HkTelemetryPkt.DeviceCount       = 0;

    /* Increment device success or error counter, none as application only */

    /* Send event success to the console */
    CFE_EVS_SendEvent(TCS_CMD_RESET_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "TCS: RESET counters command received");
    return;
}

/*
** Enable Component
** TODO: Edit for your specific component implementation
*/
void TCS_Enable(void)
{
    int32 status = OS_SUCCESS;

    /* Do any necessary checks, confirm that device is currently disabled */
    if (TCS_AppData.HkTelemetryPkt.DeviceEnabled == TCS_DEVICE_DISABLED)
    {
        /* Increment command success counter */
        TCS_AppData.HkTelemetryPkt.CommandCount++;

        /*
        ** Do the action, initialize hardware interface and set enabled
        ** TODO: Make specific to your application depending on protocol in use
        ** Note that other components provide examples for the different protocols
        */
        TCS_AppData.TcsUart.deviceString  = TCS_CFG_STRING;
        TCS_AppData.TcsUart.handle        = TCS_CFG_HANDLE;
        TCS_AppData.TcsUart.isOpen        = PORT_CLOSED;
        TCS_AppData.TcsUart.baud          = TCS_CFG_BAUDRATE_HZ;
        TCS_AppData.TcsUart.access_option = uart_access_flag_RDWR;

        status = uart_init_port(&TCS_AppData.TcsUart);
        if (status == OS_SUCCESS)
        {
            TCS_AppData.HkTelemetryPkt.DeviceEnabled = TCS_DEVICE_ENABLED;

            /* Increment device success counter */
            TCS_AppData.HkTelemetryPkt.DeviceCount++;

            /* Send device event success to the console */
            CFE_EVS_SendEvent(TCS_ENABLE_INF_EID, CFE_EVS_EventType_INFORMATION,
                              "TCS: Device enabled successfully");
        }
        else
        {
            /* Increment device error counter */
            TCS_AppData.HkTelemetryPkt.DeviceErrorCount++;

            /* Send device event failure to the console */
            CFE_EVS_SendEvent(TCS_UART_INIT_ERR_EID, CFE_EVS_EventType_ERROR,
                              "TCS: Device UART port initialization error %d", status);
        }
    }
    else
    {
        /* Increment command error count */
        TCS_AppData.HkTelemetryPkt.CommandErrorCount++;

        /* Send command event failure to the console */
        CFE_EVS_SendEvent(TCS_ENABLE_ERR_EID, CFE_EVS_EventType_ERROR,
                          "TCS: Device enable failed, already enabled");
    }
    return;
}

/*
** Disable Component
** TODO: Edit for your specific component implementation
*/
void TCS_Disable(void)
{
    int32 status = OS_SUCCESS;

    /* Do any necessary checks, confirm that device is currently enabled */
    if (TCS_AppData.HkTelemetryPkt.DeviceEnabled == TCS_DEVICE_ENABLED)
    {
        /* Increment command success counter */
        TCS_AppData.HkTelemetryPkt.CommandCount++;

        /*
        ** Do the action, close hardware interface and set disabled
        ** TODO: Make specific to your application depending on protocol in use
        ** Note that other components provide examples for the different protocols
        */
        status = uart_close_port(&TCS_AppData.TcsUart);
        if (status == OS_SUCCESS)
        {
            TCS_AppData.HkTelemetryPkt.DeviceEnabled = TCS_DEVICE_DISABLED;

            /* Increment device success counter */
            TCS_AppData.HkTelemetryPkt.DeviceCount++;

            /* Send device event success to the console */
            CFE_EVS_SendEvent(TCS_DISABLE_INF_EID, CFE_EVS_EventType_INFORMATION,
                              "TCS: Device disabled successfully");
        }
        else
        {
            /* Increment device error counter */
            TCS_AppData.HkTelemetryPkt.DeviceErrorCount++;

            /* Send device event failure to the console */
            CFE_EVS_SendEvent(TCS_UART_CLOSE_ERR_EID, CFE_EVS_EventType_ERROR,
                              "TCS: Device UART port close error %d", status);
        }
    }
    else
    {
        /* Increment command error count */
        TCS_AppData.HkTelemetryPkt.CommandErrorCount++;

        /* Send command event failure to the console */
        CFE_EVS_SendEvent(TCS_DISABLE_ERR_EID, CFE_EVS_EventType_ERROR,
                          "TCS: Device disable failed, already disabled");
    }
    return;
}

/*
** Configure Component
** TODO: Edit for your specific component implementation
*/
void TCS_Configure(void)
{
    int32                status        = OS_SUCCESS;
    int32                device_status = OS_SUCCESS;
    TCS_Config_cmd_t *config_cmd    = (TCS_Config_cmd_t *)TCS_AppData.MsgPtr;

    /* Do any necessary checks, confirm that device is currently enabled */
    if (TCS_AppData.HkTelemetryPkt.DeviceEnabled != TCS_DEVICE_ENABLED)
    {
        status = OS_ERROR;
        /* Increment command error count */
        TCS_AppData.HkTelemetryPkt.CommandErrorCount++;

        /* Send event logging failure of check to the console */
        CFE_EVS_SendEvent(TCS_CMD_CONFIG_EN_ERR_EID, CFE_EVS_EventType_ERROR,
                          "TCS: Configuration command invalid when device disabled");
    }

    /* Do any necessary checks, confirm valid configuration value */
    if (config_cmd->DeviceCfg == 0xFFFFFFFF) // 4294967295
    {
        status = OS_ERROR;
        /* Increment command error count */
        TCS_AppData.HkTelemetryPkt.CommandErrorCount++;

        /* Send event logging failure of check to the console */
        CFE_EVS_SendEvent(TCS_CMD_CONFIG_VAL_ERR_EID, CFE_EVS_EventType_ERROR,
                          "TCS: Configuration command with value %u is invalid", config_cmd->DeviceCfg);
    }

    if (status == OS_SUCCESS)
    {
        /* Increment command success counter */
        TCS_AppData.HkTelemetryPkt.CommandCount++;

        /* Do the action, command device to with a new configuration */
        device_status = TCS_CommandDevice(&TCS_AppData.TcsUart, TCS_DEVICE_CFG_CMD, config_cmd->DeviceCfg);
        if (device_status == OS_SUCCESS)
        {
            /* Increment device success counter */
            TCS_AppData.HkTelemetryPkt.DeviceCount++;

            /* Send device event success to the console */
            CFE_EVS_SendEvent(TCS_CMD_CONFIG_INF_EID, CFE_EVS_EventType_INFORMATION,
                              "TCS: Configuration command received: %u", config_cmd->DeviceCfg);
        }
        else
        {
            /* Increment device error counter */
            TCS_AppData.HkTelemetryPkt.DeviceErrorCount++;

            /* Send device event failure to the console */
            CFE_EVS_SendEvent(TCS_CMD_CONFIG_DEV_ERR_EID, CFE_EVS_EventType_ERROR,
                              "TCS: Configuration command received: %u", config_cmd->DeviceCfg);
        }
    }
    return;
}

/*
** Verify command packet length matches expected
*/
int32 TCS_VerifyCmdLength(CFE_MSG_Message_t *msg, uint16 expected_length)
{
    int32             status        = OS_SUCCESS;
    CFE_SB_MsgId_t    msg_id        = CFE_SB_INVALID_MSG_ID;
    CFE_MSG_FcnCode_t cmd_code      = 0;
    size_t            actual_length = 0;

    CFE_MSG_GetSize(msg, &actual_length);
    if (expected_length != actual_length)
    {
        CFE_MSG_GetMsgId(msg, &msg_id);
        CFE_MSG_GetFcnCode(msg, &cmd_code);

        CFE_EVS_SendEvent(TCS_LEN_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid msg length: ID = 0x%X,  CC = %d, Len = %ld, Expected = %d",
                          CFE_SB_MsgIdToValue(msg_id), cmd_code, actual_length, expected_length);

        status = OS_ERROR;

        /* Increment the command error counter upon receipt of an invalid command length */
        TCS_AppData.HkTelemetryPkt.CommandErrorCount++;
    }
    return status;
}
