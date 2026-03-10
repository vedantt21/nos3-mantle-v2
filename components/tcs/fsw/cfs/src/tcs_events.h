/************************************************************************
** File:
**    tcs_events.h
**
** Purpose:
**  Define TCS application event IDs
**
*************************************************************************/

#ifndef _TCS_EVENTS_H_
#define _TCS_EVENTS_H_

/* Standard app event IDs */
#define TCS_RESERVED_EID        0
#define TCS_STARTUP_INF_EID     1
#define TCS_LEN_ERR_EID         2
#define TCS_PIPE_ERR_EID        3
#define TCS_SUB_CMD_ERR_EID     4
#define TCS_SUB_REQ_HK_ERR_EID  5
#define TCS_PROCESS_CMD_ERR_EID 6

/* Standard command event IDs */
#define TCS_CMD_ERR_EID         10
#define TCS_CMD_NOOP_INF_EID    11
#define TCS_CMD_RESET_INF_EID   12
#define TCS_CMD_ENABLE_INF_EID  13
#define TCS_ENABLE_INF_EID      14
#define TCS_ENABLE_ERR_EID      15
#define TCS_CMD_DISABLE_INF_EID 16
#define TCS_DISABLE_INF_EID     17
#define TCS_DISABLE_ERR_EID     18

/* Device specific command event IDs */
#define TCS_CMD_CONFIG_EN_ERR_EID  20
#define TCS_CMD_CONFIG_VAL_ERR_EID 21
#define TCS_CMD_CONFIG_INF_EID     22
#define TCS_CMD_CONFIG_DEV_ERR_EID 23

/* Standard telemetry event IDs */
#define TCS_DEVICE_TLM_ERR_EID 30
#define TCS_REQ_HK_ERR_EID     31

/* Device specific telemetry event IDs */
#define TCS_REQ_DATA_ERR_EID        32
#define TCS_REQ_DATA_STATUS_ERR_EID 33

/* Hardware protocol event IDs */
#define TCS_UART_INIT_ERR_EID  40
#define TCS_UART_CLOSE_ERR_EID 41

#endif /* _TCS_EVENTS_H_ */
