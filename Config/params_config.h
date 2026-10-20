/**
 ******************************************************************************
 *@file               :   params_config.h
 *@brief              :   Single parameter page and legacy payload defaults.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef PARAMS_CONFIG_H
#define PARAMS_CONFIG_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/
/* One 2 KiB page; unchanged legacy parameter address. */
#define PARAMS_ADDRESS (0x0801E000U)
#define PARAMS_PAGE_SIZE (2048U)
#define PARAMS_RECORD_WORDS (3U)
#define PARAMS_VALID_MAGIC (0x50415231U)
/* Legacy values, stored only until ADC/protection is migrated. */
#define PARAMS_DEFAULT_UV_PROTECT_MV (10000U)
#define PARAMS_DEFAULT_UV_RECOVER_MV (12000U)

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* PARAMS_CONFIG_H */
/* end of file --------------------------------------------------------------*/
