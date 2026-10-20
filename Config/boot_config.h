/**
 ******************************************************************************
 *@file               :   boot_config.h
 *@brief              :   Legacy boot flag, application bounds and command constants.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef BOOT_CONFIG_H
#define BOOT_CONFIG_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/
/* Linker origin and vector relocation must both match before any flag write. */
#define BOOT_FLAG_ADDRESS (0x08003000U)
#define BOOT_FLAG_PAGE_SIZE (2048U)
#define BOOT_APP_ADDRESS (0x08003800U)
#define BOOT_APP_LIMIT (0x0801E000U)
#define BOOT_APP_MAX_SIZE (BOOT_APP_LIMIT - BOOT_APP_ADDRESS)
#define BOOT_FLAG_REQUEST (0x0000U)
#define BOOT_FLAG_NORMAL (0x55BBU)
#define BOOT_REQUEST_TEXT "BootLoad"
#define BOOT_REQUEST_SIZE (8U)

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* BOOT_CONFIG_H */
/* end of file --------------------------------------------------------------*/
