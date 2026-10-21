/**
 ******************************************************************************
 *@file               :   dto_config.h
 *@brief              :   Two-relay DTO controls and protection timing.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef DTO_CONFIG_H
#define DTO_CONFIG_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/
#define DTO_RELAY_COUNT (2U)
#define DTO_RELAY_ADDRESS (0x0801F000U)
/* Tagged low two bits store requested pin levels, independent of inversion. */
#define DTO_RELAY_GPIO_RECORD (0x10U)
#define DTO_UV_PROTECT_MS (3000U)
#define DTO_UV_RECOVER_MS (3000U)
#define DTO_LED_HEARTBEAT_MS (1000U)
#define DTO_LED_ACTIVITY_MS (100U)
/* Ten ASCII bytes, matching the legacy five-register version field width. */
#define DTO_FIRMWARE_VERSION "DTO_V2.0.0"

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* DTO_CONFIG_H */
/* end of file --------------------------------------------------------------*/
