/**
 ******************************************************************************
 *@file               :   bsp_system.h
 *@brief              :   Monotonic time, interrupt guards and watchdog access.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef BSP_SYSTEM_H
#define BSP_SYSTEM_H

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
uint32_t bsp_system_time_ms(void);
uint32_t bsp_system_irq_save(void);
void     bsp_system_irq_restore(uint32_t irq_state);
uint8_t  bsp_system_feed_watchdog(void);
void     bsp_system_reset(void);
uint32_t bsp_system_image_base(void);
uint32_t bsp_system_image_limit(void);
uint32_t bsp_system_vector_base(void);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* BSP_SYSTEM_H */
/* end of file --------------------------------------------------------------*/
