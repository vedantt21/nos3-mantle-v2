/*******************************************************************************
** File: tcs_device.h
**
** Purpose:
**   This is the header file for the TCS device.
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
** TODO: Make specific to your application
*/
#define TCS_DEVICE_HDR   0xDEAD
#define TCS_DEVICE_HDR_0 0xDE
#define TCS_DEVICE_HDR_1 0xAD

#define TCS_DEVICE_NOOP_CMD     0x00
#define TCS_DEVICE_REQ_HK_CMD   0x01
#define TCS_DEVICE_REQ_DATA_CMD 0x02
#define TCS_DEVICE_CFG_CMD      0x03

#define TCS_DEVICE_TRAILER   0xBEEF
#define TCS_DEVICE_TRAILER_0 0xBE
#define TCS_DEVICE_TRAILER_1 0xEF

#define TCS_DEVICE_HDR_TRL_LEN 4
#define TCS_DEVICE_CMD_SIZE    9

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
    uint16_t DeviceDataX;
    uint16_t DeviceDataY;
    uint16_t DeviceDataZ;

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
