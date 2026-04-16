/*******************************************************************************
** File: tcs_device.h
**
** Purpose:
**   This is the header file for the TCS device.
**
** Implementation Notes:
**   This file is the single source of truth for the UART wire protocol used by
**   the cFS app and the NOS3 TCS hardware model.  Recreating the simulator/app
**   handshake requires keeping these byte values and packed structures aligned
**   with tcs_hardware_model.cpp.
**
*******************************************************************************/
#ifndef _TCS_DEVICE_H_
#define _TCS_DEVICE_H_

/*
** Required header files.
*/
#include "device_cfg.h"
#include "hwlib.h"

#ifndef TCS_CFG
#include "tcs_platform_cfg.h"
#endif

/*
** Type definitions
** Every UART transaction starts with 0xDEAD and ends with 0xBEEF.  The app
** sends 9-byte command frames:
**   [DE AD] [opcode] [payload byte 3] [payload byte 2] [payload byte 1]
**   [payload byte 0] [BE EF]
** The simulator echoes the command frame first, then sends HK or data bytes
** when the opcode requests telemetry.
*/
#define TCS_DEVICE_HDR   0xDEAD
#define TCS_DEVICE_HDR_0 0xDE
#define TCS_DEVICE_HDR_1 0xAD

#define TCS_DEVICE_NOOP_CMD     0x00
#define TCS_DEVICE_REQ_HK_CMD   0x01
#define TCS_DEVICE_REQ_DATA_CMD 0x02
#define TCS_DEVICE_CFG_CMD      0x03
/* Opcode 0x04 writes MANUAL(0) or AUTO(1) into the simulator control mode. */
#define TCS_DEVICE_SET_MODE_CMD 0x04
/* Opcode 0x05 writes OFF(0) or ON(1) into the simulator heater state. */
#define TCS_DEVICE_SET_HEATER_CMD 0x05

#define TCS_DEVICE_TRAILER   0xBEEF
#define TCS_DEVICE_TRAILER_0 0xBE
#define TCS_DEVICE_TRAILER_1 0xEF

#define TCS_DEVICE_HDR_TRL_LEN 4
#define TCS_DEVICE_CMD_SIZE    9

#define TCS_HEATER_STATE_OFF 0
#define TCS_HEATER_STATE_ON  1

#define TCS_CONTROL_MODE_MANUAL 0
#define TCS_CONTROL_MODE_AUTO   1

/*
** Temperatures are now consistently carried in Kelvin.  The simulator uses
** double precision internally, but the UART data packet sends 32-bit floats for
** the temperatures and signed 16-bit integers for thresholds.
*/
#define TCS_INITIAL_SKIN_TEMPERATURE_K     293.0f
#define TCS_INITIAL_INTERNAL_TEMPERATURE_K 293.0f
#define TCS_LOWER_THRESHOLD_K              273
#define TCS_UPPER_THRESHOLD_K              283

/*
** TCS device housekeeping telemetry definition
*/
typedef struct
{
    uint32_t DeviceCounter;
    uint32_t DeviceConfig;
    uint32_t DeviceStatus;

} __attribute__((packed)) TCS_Device_HK_tlm_t;
#define TCS_DEVICE_HK_LNGTH sizeof(TCS_Device_HK_tlm_t)
#define TCS_DEVICE_HK_SIZE  TCS_DEVICE_HK_LNGTH + TCS_DEVICE_HDR_TRL_LEN

/*
** TCS device data telemetry definition
*/
typedef struct
{
    uint32_t DeviceCounter;
    /* Internal node temperature from the two-node thermal model, in Kelvin. */
    float    CurrentTemperatureK;
    /* AUTO mode turns the heater on below LowerThresholdK. */
    int16_t  LowerThresholdK;
    /* AUTO mode turns the heater off above UpperThresholdK. */
    int16_t  UpperThresholdK;
    /* Raw simulator state: 0 = OFF, 1 = ON. */
    uint8_t  HeaterState;
    /* Raw simulator mode: 0 = MANUAL, 1 = AUTO. */
    uint8_t  ControlMode;
    /* External skin/shell node temperature from the orbital model, in Kelvin. */
    float    SkinTemperatureK;

} __attribute__((packed)) TCS_Device_Data_tlm_t;
#define TCS_DEVICE_DATA_LNGTH sizeof(TCS_Device_Data_tlm_t)
#define TCS_DEVICE_DATA_SIZE  TCS_DEVICE_DATA_LNGTH + TCS_DEVICE_HDR_TRL_LEN

/*
** Prototypes
*/
int32_t TCS_ReadData(uart_info_t *device, uint8_t *read_data, uint8_t data_length);
int32_t TCS_CommandDevice(uart_info_t *device, uint8_t cmd, uint32_t payload);
int32_t TCS_RequestHK(uart_info_t *device, TCS_Device_HK_tlm_t *data);
int32_t TCS_RequestData(uart_info_t *device, TCS_Device_Data_tlm_t *data);

#endif /* _TCS_DEVICE_H_ */
