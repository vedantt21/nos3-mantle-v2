#include "utgenstub.h"
#include "tcs_device.h"

int32_t TCS_ReadData(uart_info_t *device, uint8_t *read_data, uint8_t data_length)
{
    UT_GenStub_SetupReturnBuffer(TCS_ReadData, int32_t);

    UT_GenStub_AddParam(TCS_ReadData, uart_info_t *, device);
    UT_GenStub_AddParam(TCS_ReadData, uint8_t *, read_data);
    UT_GenStub_AddParam(TCS_ReadData, uint8_t, data_length);

    UT_GenStub_Execute(TCS_ReadData, Basic, NULL);

    return UT_GenStub_GetReturnValue(TCS_ReadData, int32_t);
}

int32_t TCS_CommandDevice(uart_info_t *device, uint8_t cmd, uint32_t payload)
{
    UT_GenStub_SetupReturnBuffer(TCS_CommandDevice, int32_t);

    UT_GenStub_AddParam(TCS_CommandDevice, uart_info_t *, device);
    UT_GenStub_AddParam(TCS_CommandDevice, uint8_t, cmd);
    UT_GenStub_AddParam(TCS_CommandDevice, uint32_t, payload);

    UT_GenStub_Execute(TCS_CommandDevice, Basic, NULL);

    return UT_GenStub_GetReturnValue(TCS_CommandDevice, int32_t);
}

int32_t TCS_RequestHK(uart_info_t *device, TCS_Device_HK_tlm_t *data)
{
    UT_GenStub_SetupReturnBuffer(TCS_RequestHK, int32_t);

    UT_GenStub_AddParam(TCS_RequestHK, uart_info_t *, device);
    UT_GenStub_AddParam(TCS_RequestHK, TCS_Device_HK_tlm_t *, data);

    UT_GenStub_Execute(TCS_RequestHK, Basic, NULL);

    return UT_GenStub_GetReturnValue(TCS_RequestHK, int32_t);
}

int32_t TCS_RequestData(uart_info_t *device, TCS_Device_Data_tlm_t *data)
{
    UT_GenStub_SetupReturnBuffer(TCS_RequestData, int32_t);

    UT_GenStub_AddParam(TCS_RequestData, uart_info_t *, device);
    UT_GenStub_AddParam(TCS_RequestData, TCS_Device_Data_tlm_t *, data);

    UT_GenStub_Execute(TCS_RequestData, Basic, NULL);

    return UT_GenStub_GetReturnValue(TCS_RequestData, int32_t);
}
