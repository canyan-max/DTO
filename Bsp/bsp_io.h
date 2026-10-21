/**
 ******************************************************************************
 *@file               :   bsp_io.h
 *@brief              :   Board power and relay operations.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef BSP_IO_H
#define BSP_IO_H

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
/* Before MX_GPIO_Init: enable port clock and preload ODR, without mode changes.
 */
uint8_t bsp_io_preload_relays(uint8_t gpio_mask);
/* Relay index: 0..1 only. State is GPIO level, not load/contact status. */
uint8_t bsp_io_set_relay(uint8_t relay, uint8_t enabled);
/* Index 0=work, 1=485_1, 2=485_2. Active low LEDs. */
uint8_t bsp_io_set_led(uint8_t led, uint8_t enabled);
/* Active-high enables from reference board; wiring needs bench verification. */
uint8_t bsp_io_set_power(uint8_t module_4g, uint8_t rs485_1, uint8_t rs485_2);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* BSP_IO_H */
/* end of file --------------------------------------------------------------*/
