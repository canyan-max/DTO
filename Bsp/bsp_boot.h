/**
 ******************************************************************************
 *@file               :   bsp_boot.h
 *@brief              :   Guard and commit the dedicated legacy boot flag page.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef BSP_BOOT_H
#define BSP_BOOT_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
/* No write is allowed until linked image bounds and live VTOR match Config. */
uint8_t bsp_boot_layout_valid(void);
uint8_t bsp_boot_read_flag(uint64_t *p_word);
uint8_t bsp_boot_write_flag(uint16_t flag);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* BSP_BOOT_H */
/* end of file --------------------------------------------------------------*/
