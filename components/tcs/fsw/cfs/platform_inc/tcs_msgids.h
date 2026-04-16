/************************************************************************
** File:
**   $Id: tcs_msgids.h  $
**
** Purpose:
**  Define TCS Message IDs
**
** Implementation Notes:
**  These MIDs bind the flight app, OpenC3 command files, and XTCE together.
**  Command MID 0x18F1 carries TCS_NOOP/RESET/HEATER/ENABLE/DISABLE/CONFIG
**  function codes. Request MID 0x18F2 carries telemetry request function codes.
**  Telemetry MIDs 0x08F1 and 0x08F2 must stay aligned with TCS_TLM.txt and
**  tcs.xtce, otherwise ground will decode the wrong packet.
**
*************************************************************************/
#ifndef _TCS_MSGIDS_H_
#define _TCS_MSGIDS_H_

/*
** CCSDS V1 Command Message IDs (MID) must be 0x18xx
*/
#define TCS_CMD_MID 0x18F1 /* Ground commands with TCS_*_CC function codes */

/*
** This MID is for commands telling the app to publish its telemetry message
*/
#define TCS_REQ_HK_MID 0x18F2 /* Requests for HK/data telemetry publication */

/*
** CCSDS V1 Telemetry Message IDs must be 0x08xx
*/
#define TCS_HK_TLM_MID     0x08F1 /* App housekeeping packet */
#define TCS_DEVICE_TLM_MID 0x08F2 /* Thermal/device data packet */

#endif /* _TCS_MSGIDS_H_ */
