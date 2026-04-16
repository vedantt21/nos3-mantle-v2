/*******************************************************************************
** File: tcs_device.c
**
** Purpose:
**   This file contains the source code for the TCS device.
**
** Implementation Notes:
**   The functioning TCS model made the device packet a direct mirror of the
**   simulator's thermal state: current/internal temperature, Kelvin thresholds,
**   heater state, control mode, and skin temperature.  All multi-byte fields on
**   the UART link are sent most-significant byte first, so this file manually
**   reconstructs integers and floats before cFS publishes telemetry.
**
*******************************************************************************/

/*
** Include Files
*/
#include "tcs_device.h"
#include <string.h>

static float TCS_UnpackFloatBE(const uint8_t *data)
{
    uint32_t raw = 0;
    float value = 0.0f;

    /*
    ** Rebuild the four UART bytes into the exact IEEE-754 bit pattern produced
    ** by the simulator.  memcpy is used instead of a pointer cast so strict
    ** aliasing rules and alignment do not corrupt the float conversion.
    */
    raw = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | ((uint32_t)data[3]);
    memcpy(&value, &raw, sizeof(value));

    return value;
}

/*
** Generic read data from device
*/
int32_t TCS_ReadData(uart_info_t *device, uint8_t *read_data, uint8_t data_length)
{
    int32_t status             = OS_SUCCESS;
    int32_t bytes              = 0;
    int32_t bytes_available    = 0;
    uint8_t ms_timeout_counter = 0;

    /* Wait until all data received or timeout occurs */
    bytes_available = uart_bytes_available(device);
    while ((bytes_available < data_length) && (ms_timeout_counter < TCS_CFG_MS_TIMEOUT))
    {
        ms_timeout_counter++;
        OS_TaskDelay(1);
        bytes_available = uart_bytes_available(device);
    }

    if (ms_timeout_counter < TCS_CFG_MS_TIMEOUT)
    {
        /* Limit bytes available */
        if (bytes_available > data_length)
        {
            bytes_available = data_length;
        }

        /* Read data */
        bytes = uart_read_port(device, read_data, bytes_available);
        if (bytes != bytes_available)
        {
#ifdef TCS_CFG_DEBUG
            OS_printf("  TCS_ReadData: Bytes read != to requested! \n");
#endif
            status = OS_ERROR;
        } /* uart_read */
    }
    else
    {
        status = OS_ERROR;
    } /* ms_timeout_counter */

    return status;
}

/*
** Generic command to device
** Note that confirming the echoed response is specific to this implementation
*/
int32_t TCS_CommandDevice(uart_info_t *device, uint8_t cmd_code, uint32_t payload)
{
    int32_t status = OS_SUCCESS;
    int32_t bytes  = 0;
    uint8_t write_data[TCS_DEVICE_CMD_SIZE];
    uint8_t read_data[TCS_DEVICE_DATA_SIZE];

    /*
    ** Prepare the 9-byte command frame expected by tcs_hardware_model.cpp:
    ** 0xDEAD header, one opcode byte, four big-endian payload bytes, then
    ** 0xBEEF trailer.  For mode/heater commands only the final payload byte is
    ** meaningful, but keeping a 32-bit payload makes config and future commands
    ** share the same frame shape.
    */
    write_data[0] = TCS_DEVICE_HDR_0;
    write_data[1] = TCS_DEVICE_HDR_1;
    write_data[2] = cmd_code;
    write_data[3] = payload >> 24;
    write_data[4] = payload >> 16;
    write_data[5] = payload >> 8;
    write_data[6] = payload;
    write_data[7] = TCS_DEVICE_TRAILER_0;
    write_data[8] = TCS_DEVICE_TRAILER_1;

    /* Flush any prior data */
    status = uart_flush(device);
    if (status == UART_SUCCESS)
    {
        /*
        ** The simulator echoes a valid command frame before returning any HK or
        ** data packet.  The echo check below is the app-side acknowledgement
        ** that the simulator accepted the opcode and payload.
        */
        bytes = uart_write_port(device, write_data, TCS_DEVICE_CMD_SIZE);
#ifdef TCS_CFG_DEBUG
        OS_printf("  TCS_CommandDevice[%d] = ", bytes);
        for (uint32_t i = 0; i < TCS_DEVICE_CMD_SIZE; i++)
        {
            OS_printf("%02x", write_data[i]);
        }
        OS_printf("\n");
#endif
        if (bytes == TCS_DEVICE_CMD_SIZE)
        {
            status = TCS_ReadData(device, read_data, TCS_DEVICE_CMD_SIZE);
            if (status == OS_SUCCESS)
            {
                /* Confirm echoed response */
                bytes = 0;
                while ((bytes < (int32_t)TCS_DEVICE_CMD_SIZE) && (status == OS_SUCCESS))
                {
                    if (read_data[bytes] != write_data[bytes])
                    {
                        status = OS_ERROR;
                    }
                    bytes++;
                }
            } /* TCS_ReadData */
            else
            {
#ifdef TCS_CFG_DEBUG
                OS_printf("TCS_CommandDevice - TCS_ReadData returned %d \n", status);
#endif
            }
        }
        else
        {
#ifdef TCS_CFG_DEBUG
            OS_printf("TCS_CommandDevice - uart_write_port returned %d, expected %d \n", bytes,
                      TCS_DEVICE_CMD_SIZE);
#endif
        } /* uart_write */
    }     /* uart_flush*/
    return status;
}

/*
** Request housekeeping command
*/
int32_t TCS_RequestHK(uart_info_t *device, TCS_Device_HK_tlm_t *data)
{
    int32_t status = OS_SUCCESS;
    uint8_t read_data[TCS_DEVICE_HK_SIZE];

    /* Command device to send HK */
    status = TCS_CommandDevice(device, TCS_DEVICE_REQ_HK_CMD, 0);
    if (status == OS_SUCCESS)
    {
        /* Read HK data */
        status = TCS_ReadData(device, read_data, sizeof(read_data));
        if (status == OS_SUCCESS)
        {
#ifdef TCS_CFG_DEBUG
            OS_printf("  TCS_RequestHK = ");
            for (uint32_t i = 0; i < sizeof(read_data); i++)
            {
                OS_printf("%02x", read_data[i]);
            }
            OS_printf("\n");
#endif

            /* Verify data header and trailer */
            if ((read_data[0] == TCS_DEVICE_HDR_0) && (read_data[1] == TCS_DEVICE_HDR_1) &&
                (read_data[14] == TCS_DEVICE_TRAILER_0) && (read_data[15] == TCS_DEVICE_TRAILER_1))
            {
                data->DeviceCounter = read_data[2] << 24;
                data->DeviceCounter |= read_data[3] << 16;
                data->DeviceCounter |= read_data[4] << 8;
                data->DeviceCounter |= read_data[5];

                data->DeviceConfig = read_data[6] << 24;
                data->DeviceConfig |= read_data[7] << 16;
                data->DeviceConfig |= read_data[8] << 8;
                data->DeviceConfig |= read_data[9];

                data->DeviceStatus = read_data[10] << 24;
                data->DeviceStatus |= read_data[11] << 16;
                data->DeviceStatus |= read_data[12] << 8;
                data->DeviceStatus |= read_data[13];

#ifdef TCS_CFG_DEBUG
                OS_printf("  Header  = 0x%02x%02x  \n", read_data[0], read_data[1]);
                OS_printf("  Counter = 0x%08x      \n", data->DeviceCounter);
                OS_printf("  Config  = 0x%08x      \n", data->DeviceConfig);
                OS_printf("  Status  = 0x%08x      \n", data->DeviceStatus);
                OS_printf("  Trailer = 0x%02x%02x  \n", read_data[14], read_data[15]);
#endif
            }
            else
            {
#ifdef TCS_CFG_DEBUG
                OS_printf("  TCS_RequestHK: TCS_ReadData reported error %d \n", status);
#endif
                status = OS_ERROR;
            }
        } /* TCS_ReadData */
    }
    else
    {
#ifdef TCS_CFG_DEBUG
        OS_printf("  TCS_RequestHK: TCS_CommandDevice reported error %d \n", status);
#endif
    }
    return status;
}

/*
** Request data command
*/
int32_t TCS_RequestData(uart_info_t *device, TCS_Device_Data_tlm_t *data)
{
    int32_t status = OS_SUCCESS;
    uint8_t read_data[TCS_DEVICE_DATA_SIZE];

    /*
    ** Ask for the 22-byte data packet.  The command echo is consumed by
    ** TCS_CommandDevice(); the following read consumes only the telemetry:
    ** [DE AD] counter currentK lowerK upperK heater mode skinK [BE EF].
    */
    status = TCS_CommandDevice(device, TCS_DEVICE_REQ_DATA_CMD, 0);
    if (status == OS_SUCCESS)
    {
        /* Read the simulator data packet after the command echo. */
        status = TCS_ReadData(device, read_data, sizeof(read_data));
        if (status == OS_SUCCESS)
        {
#ifdef TCS_CFG_DEBUG
            OS_printf("  TCS_RequestData = ");
            for (uint32_t i = 0; i < sizeof(read_data); i++)
            {
                OS_printf("%02x", read_data[i]);
            }
            OS_printf("\n");
#endif

            /*
            ** Verify data header and trailer before unpacking by offset.  If
            ** either sentinel is wrong, the packet could be shifted and every
            ** field below would be decoded into the wrong telemetry item.
            */
            if ((read_data[0] == TCS_DEVICE_HDR_0) && (read_data[1] == TCS_DEVICE_HDR_1) &&
                (read_data[20] == TCS_DEVICE_TRAILER_0) && (read_data[21] == TCS_DEVICE_TRAILER_1))
            {
                /*
                ** Byte map for recreation:
                **   02-05: device counter, uint32, big-endian
                **   06-09: internal/current temperature, IEEE-754 float, K
                **   10-11: lower AUTO threshold, int16, K
                **   12-13: upper AUTO threshold, int16, K
                **   14:    heater state, 0 OFF / 1 ON
                **   15:    control mode, 0 MANUAL / 1 AUTO
                **   16-19: skin temperature, IEEE-754 float, K
                */
                data->DeviceCounter = read_data[2] << 24;
                data->DeviceCounter |= read_data[3] << 16;
                data->DeviceCounter |= read_data[4] << 8;
                data->DeviceCounter |= read_data[5];

                data->CurrentTemperatureK = TCS_UnpackFloatBE(&read_data[6]);
                data->LowerThresholdK = (int16_t)((read_data[10] << 8) | read_data[11]);
                data->UpperThresholdK = (int16_t)((read_data[12] << 8) | read_data[13]);
                data->HeaterState = read_data[14];
                data->ControlMode = read_data[15];
                data->SkinTemperatureK = TCS_UnpackFloatBE(&read_data[16]);

#ifdef TCS_CFG_DEBUG
                OS_printf("  Header  = 0x%02x%02x  \n", read_data[0], read_data[1]);
                OS_printf("  Counter = 0x%08x, %d  \n", data->DeviceCounter, data->DeviceCounter);
                OS_printf("  Current Temperature = %.3f K  \n", (double)data->CurrentTemperatureK);
                OS_printf("  Lower Threshold     = %d K  \n", (int)data->LowerThresholdK);
                OS_printf("  Upper Threshold     = %d K  \n", (int)data->UpperThresholdK);
                OS_printf("  Heater State        = %u    \n", (unsigned int)data->HeaterState);
                OS_printf("  Control Mode        = %u    \n", (unsigned int)data->ControlMode);
                OS_printf("  Skin Temperature    = %.3f K  \n", (double)data->SkinTemperatureK);
                OS_printf("  Trailer = 0x%02x%02x  \n", read_data[20], read_data[21]);
#endif
            }
            else
            {
#ifdef TCS_CFG_DEBUG
                OS_printf("  TCS_RequestData: Invalid header or trailer! \n");
#endif
                status = OS_ERROR;
            }
        }
        else
        {
#ifdef TCS_CFG_DEBUG
            OS_printf("  TCS_RequestData: Invalid data read! \n");
#endif
            status = OS_ERROR;
        } /* TCS_ReadData */
    }
    else
    {
#ifdef TCS_CFG_DEBUG
        OS_printf("  TCS_RequestData: TCS_CommandDevice reported error %d \n", status);
#endif
    }
    return status;
}
