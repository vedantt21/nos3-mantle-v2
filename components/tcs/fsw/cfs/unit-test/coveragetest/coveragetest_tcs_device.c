
#include "tcs_app_coveragetest_common.h"

/*
 * Implementation notes:
 * These tests exercise the UART device adapter after the functioning thermal
 * model changed the data packet fields.  The important recreation detail is the
 * 22-byte data packet layout:
 *   0-1   header 0xDEAD
 *   2-5   counter
 *   6-9   current/internal temperature float
 *   10-11 lower threshold Kelvin
 *   12-13 upper threshold Kelvin
 *   14    heater state
 *   15    control mode
 *   16-19 skin temperature float
 *   20-21 trailer 0xBEEF
 */

void Test_TCS_ReadData(void)
{
    uart_info_t device;
    uint8_t     read_data[8];
    uint8_t     data_length = 8;
    TCS_ReadData(&device, read_data, data_length);

    UT_SetDeferredRetcode(UT_KEY(uart_bytes_available), 1, data_length);
    TCS_ReadData(&device, read_data, data_length);

    UT_SetDeferredRetcode(UT_KEY(uart_bytes_available), 1, data_length + 1);
    TCS_ReadData(&device, read_data, data_length);
}

void Test_TCS_CommandDevice(void)
{
    uart_info_t device;
    uint8_t     cmd_code = 0;
    uint32_t    payload  = 0;
    TCS_CommandDevice(&device, cmd_code, payload);

    UT_SetDeferredRetcode(UT_KEY(uart_flush), 1, UART_ERROR);
    TCS_CommandDevice(&device, cmd_code, payload);

    UT_SetDeferredRetcode(UT_KEY(uart_write_port), 1, TCS_DEVICE_CMD_SIZE);
    TCS_CommandDevice(&device, cmd_code, payload);

    UT_SetDeferredRetcode(UT_KEY(uart_write_port), 1, TCS_DEVICE_CMD_SIZE);
    UT_SetDeferredRetcode(UT_KEY(uart_bytes_available), 1, 9);
    UT_SetDeferredRetcode(UT_KEY(uart_read_port), 1, 9);
    UT_SetDefaultReturnValue(UT_KEY(TCS_ReadData), OS_SUCCESS);
    UT_SetDeferredRetcode(UT_KEY(TCS_ReadData), 1, OS_SUCCESS);
    TCS_CommandDevice(&device, cmd_code, payload);
}

void Test_TCS_RequestHK(void)
{
    uart_info_t            device;
    TCS_Device_HK_tlm_t data;
    TCS_RequestHK(&device, &data);

    uint8_t read_data[] = {0xDE, 0xAD, 0x00, 0x00, 0x00, 0x07, 0x00, 0x06,
                           0x00, 0x0C, 0x00, 0x12, 0x00, 0x00, 0xBE, 0xEF};
    UT_SetDeferredRetcode(UT_KEY(uart_bytes_available), 1, 16);
    UT_SetDeferredRetcode(UT_KEY(uart_read_port), 1, 16);
    UT_SetDataBuffer(UT_KEY(uart_read_port), &read_data, sizeof(read_data), false);
    TCS_RequestHK(&device, &data);

    UT_SetDeferredRetcode(UT_KEY(uart_flush), 1, OS_ERROR);
    TCS_RequestHK(&device, &data);
}

void Test_TCS_RequestData(void)
{
    uart_info_t              device;
    TCS_Device_Data_tlm_t data;
    TCS_RequestData(&device, &data);

    /*
     * Valid data packet:
     *   0x43929333 is the current/internal temperature float bits.
     *   0x00FA and 0x0140 are Kelvin thresholds used to prove K fields decode.
     *   The final float is skin temperature, replacing the old ambient field.
     */
    uint8_t read_data[] = {0xDE, 0xAD, 0x00, 0x00, 0x00, 0x07,
                           0x43, 0x92, 0x93, 0x33,
                           0x00, 0xFA,
                           0x01, 0x40,
                           0x01,
                           0x01,
                           0x43, 0x88, 0x80, 0x00,
                           0xBE, 0xEF};
    UT_SetDeferredRetcode(UT_KEY(uart_bytes_available), 1, 22);
    UT_SetDeferredRetcode(UT_KEY(uart_read_port), 1, 22);
    UT_SetDataBuffer(UT_KEY(uart_read_port), &read_data, sizeof(read_data), false);
    TCS_RequestData(&device, &data);

    /*
     * Same payload with an invalid trailer. This must fail before any caller
     * trusts the byte offsets, which protects against shifted/corrupt packets.
     */
    uint8_t invalid_read_data[] = {0xDE, 0xAD, 0x00, 0x00, 0x00, 0x07,
                                   0x43, 0x92, 0x93, 0x33,
                                   0x00, 0xFA,
                                   0x01, 0x40,
                                   0x01,
                                   0x01,
                                   0x43, 0x88, 0x80, 0x00,
                                   0x00, 0x00};
    UT_SetDeferredRetcode(UT_KEY(uart_bytes_available), 1, 22);
    UT_SetDeferredRetcode(UT_KEY(uart_read_port), 1, 22);
    UT_SetDataBuffer(UT_KEY(uart_read_port), &invalid_read_data, sizeof(invalid_read_data), false);
    TCS_RequestData(&device, &data);

    UT_SetDeferredRetcode(UT_KEY(uart_flush), 1, OS_ERROR);
    TCS_RequestData(&device, &data);
}

void Test_TCS_RequestData_Hook(void *UserObj, UT_EntryKey_t FuncKey, const UT_StubContext_t *Context, va_list va) {}

/*
 * Setup function prior to every test
 */
void Tcs_UT_Setup(void)
{
    UT_ResetState(0);
}

/*
 * Teardown function after every test
 */
void Tcs_UT_TearDown(void) {}

/*
 * Register the test cases to execute with the unit test tool
 */
void UtTest_Setup(void)
{
    UT_SetVaHandlerFunction(UT_KEY(Test_TCS_RequestData), Test_TCS_RequestData_Hook, NULL);
    ADD_TEST(TCS_ReadData);
    ADD_TEST(TCS_CommandDevice);
    ADD_TEST(TCS_RequestHK);
    ADD_TEST(TCS_RequestData);
}
