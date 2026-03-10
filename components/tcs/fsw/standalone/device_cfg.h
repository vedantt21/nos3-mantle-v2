#ifndef _TCS_CHECKOUT_DEVICE_CFG_H_
#define _TCS_CHECKOUT_DEVICE_CFG_H_

/*
** TCS Checkout Configuration
*/
#define TCS_CFG
/* Note: NOS3 uart requires matching handle and bus number */
#define TCS_CFG_STRING      "/dev/usart_16"
#define TCS_CFG_HANDLE      16
#define TCS_CFG_BAUDRATE_HZ 115200
#define TCS_CFG_MS_TIMEOUT  250
#define TCS_CFG_DEBUG

#endif /* _TCS_CHECKOUT_DEVICE_CFG_H_ */
