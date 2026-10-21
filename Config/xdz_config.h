/**
 ******************************************************************************
 *@file               :   xdz_config.h
 *@brief              :   XDZ host/device roles and legacy wire constants.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef XDZ_CONFIG_H
#define XDZ_CONFIG_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "comm_config.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

#define XDZ_FIRMWARE_VERSION "XDZ_V2.0.0"
#define XDZ_DEVICE_PORT (COMM_PORT_RS485_1)
#define XDZ_HOST_PORT (COMM_PORT_RS485_2)
#define XDZ_DEVICE_ADDRESS (0x05U)
#define XDZ_HOST_ADDRESS (0x02U)
#define XDZ_CACHE_STALE_MS (7000U)
#define XDZ_REPLY_QUEUE_MS (1000U)
#define XDZ_FIRST_REGISTER (0x0200U)
#define XDZ_LAST_REGISTER (0x021AU)
#define XDZ_RELAY_REGISTER (0x0425U)
#define XDZ_MAX_REPLY (59U)
/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* XDZ_CONFIG_H */
/* end of file --------------------------------------------------------------*/
