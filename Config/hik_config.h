/**
 ******************************************************************************
 *@file               :   hik_config.h
 *@brief              :   HIK host port and external measurement freshness.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef HIK_CONFIG_H
#define HIK_CONFIG_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "comm_config.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/
#define HIK_FIRMWARE_VERSION "HIK_V2.0.0"
#define HIK_HOST_PORT (COMM_PORT_RS485_1)
#define HIK_DEVICE_PORT (COMM_PORT_RS485_2)
/* EVE has five live queries at >=1s intervals. Allow a complete cycle plus
 * response/recovery jitter; a 5s TTL expired healthy solar data every cycle.
 * Polling still changes profile after 5s without any valid device response. */
#define HIK_CACHE_STALE_MS (7000U)
#define HIK_REPLY_QUEUE_TIMEOUT_MS (1000U)
/* Preserve the reference packet's fixed metadata in zero-data replies. */
#define HIK_EMPTY_BATTERY_VERSION (0x10U)
#define HIK_EMPTY_SOLAR_NUMBER (1U)

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* HIK_CONFIG_H */
/* end of file --------------------------------------------------------------*/
