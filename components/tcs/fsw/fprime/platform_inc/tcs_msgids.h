/************************************************************************
** File:
**   $Id: tcs_msgids.h  $
**
** Purpose:
**  Define TCS Message IDs
**
*************************************************************************/
#ifndef _TCS_MSGIDS_H_
#define _TCS_MSGIDS_H_

/*
** CCSDS V1 Command Message IDs (MID) must be 0x18xx
*/
#define TCS_CMD_MID 0x18FA /* TODO: Change this for your app */

/*
** This MID is for commands telling the app to publish its telemetry message
*/
#define TCS_REQ_HK_MID 0x18FB /* TODO: Change this for your app */

/*
** CCSDS V1 Telemetry Message IDs must be 0x08xx
*/
#define TCS_HK_TLM_MID     0x08FA /* TODO: Change this for your app */
#define TCS_DEVICE_TLM_MID 0x08FB /* TODO: Change this for your app */

#endif /* _TCS_MSGIDS_H_ */
