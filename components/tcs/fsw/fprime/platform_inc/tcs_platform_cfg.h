/************************************************************************
** File:
**   $Id: tcs_platform_cfg.h  $
**
** Purpose:
**  Define tcs Platform Configuration Parameters
**
** Notes:
**
*************************************************************************/
#ifndef _TCS_PLATFORM_CFG_H_
#define _TCS_PLATFORM_CFG_H_

/*
** Default TCS Configuration
*/
#ifndef TCS_CFG
/* Notes:
**   NOS3 uart requires matching handle and bus number
*/
#define TCS_CFG_STRING      "usart_16"
#define TCS_CFG_HANDLE      16
#define TCS_CFG_BAUDRATE_HZ 115200
#define TCS_CFG_MS_TIMEOUT  50 /* Max 255 */
/* Note: Debug flag disabled (commented out) by default */
//#define TCS_CFG_DEBUG
#endif

#endif /* _TCS_PLATFORM_CFG_H_ */
