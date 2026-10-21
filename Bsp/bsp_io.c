/**
 ******************************************************************************
 *@file               :   bsp_io.c
 *@brief              :   Board power and relay operations.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "main.h"
#include "bsp_io.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* private  functions  ------------------------------------------------------*/

/* exported functions -------------------------------------------------------*/
uint8_t bsp_io_preload_relays(uint8_t gpio_mask)
{
    if(gpio_mask > 3U)
    {
        return 0U;
    }
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /* Boot hands these pins over as analog inputs. Set the output latches
     * now, so MX_GPIO_Init enables the remembered level on its first drive. */
    HAL_GPIO_WritePin(G2ROUT1_GPIO_Port, G2ROUT1_Pin,
                      0U != (gpio_mask & 1U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(G2ROUT2_GPIO_Port, G2ROUT2_Pin,
                      0U != (gpio_mask & 2U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return 1U;
}
uint8_t bsp_io_set_power(uint8_t module_4g, uint8_t rs485_1, uint8_t rs485_2)
{
    if(module_4g > 1U || rs485_1 > 1U || rs485_2 > 1U)
    {
        return 0U;
    }
    HAL_GPIO_WritePin(GM800FVDDEN_GPIO_Port, GM800FVDDEN_Pin,
                      0U != module_4g ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(VCCEN1_GPIO_Port, VCCEN1_Pin,
                      0U != rs485_1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(VCCEN2_GPIO_Port, VCCEN2_Pin,
                      0U != rs485_2 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return 1U;
}
uint8_t bsp_io_set_relay(uint8_t relay, uint8_t enabled)
{
    static GPIO_TypeDef *const p_ports[] = {
        G2ROUT1_GPIO_Port,
        G2ROUT2_GPIO_Port,
    };
    static const uint16_t g_pins[] = {
        G2ROUT1_Pin,
        G2ROUT2_Pin,
    };
    if(relay >= sizeof(g_pins) / sizeof(g_pins[0U]) || enabled > 1U)
    {
        return 0U;
    }
    HAL_GPIO_WritePin(p_ports[relay], g_pins[relay],
                      0U != enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return 1U;
}
uint8_t bsp_io_set_led(uint8_t led, uint8_t enabled)
{
    static GPIO_TypeDef *const p_ports[] = {
        LED1_GPIO_Port,
        LED2_GPIO_Port,
        LED3_GPIO_Port,
    };
    static const uint16_t g_pins[] = {LED1_Pin, LED2_Pin, LED3_Pin};
    if(led >= 3U || enabled > 1U)
    {
        return 0U;
    }
    HAL_GPIO_WritePin(p_ports[led], g_pins[led],
                      0U != enabled ? GPIO_PIN_RESET : GPIO_PIN_SET);
    return 1U;
}

/* end of file --------------------------------------------------------------*/
