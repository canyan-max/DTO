/**
 ******************************************************************************
 *@file               :   bsp_system.c
 *@brief              :   Monotonic time, interrupt guards and watchdog access.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "iwdg.h"
#include "bsp_system.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/
#if defined(__CC_ARM)
extern uint32_t Load$$LR$$LR_IROM1$$Base;
extern uint32_t Load$$LR$$LR_IROM1$$Limit;
#endif

/* private  functions  ------------------------------------------------------*/

/* exported functions -------------------------------------------------------*/
uint32_t bsp_system_time_ms(void)
{
    return HAL_GetTick();
}
uint32_t bsp_system_irq_save(void)
{
    uint32_t irq_state = __get_PRIMASK();
    __disable_irq();
    __DMB();
    return irq_state;
}
void bsp_system_irq_restore(uint32_t irq_state)
{
    __DMB();
    __set_PRIMASK(irq_state);
}
uint8_t bsp_system_feed_watchdog(void)
{
    return HAL_OK == HAL_IWDG_Refresh(&hiwdg);
}

void bsp_system_reset(void)
{
    NVIC_SystemReset();
}
/* Load region includes the Flash copy of initialized RAM, not only RO code. */
uint32_t bsp_system_image_base(void)
{
#if defined(__CC_ARM)
    return (uint32_t)&Load$$LR$$LR_IROM1$$Base;
#else
    return 0U;
#endif
}
uint32_t bsp_system_image_limit(void)
{
#if defined(__CC_ARM)
    return (uint32_t)&Load$$LR$$LR_IROM1$$Limit;
#else
    return 0U;
#endif
}
uint32_t bsp_system_vector_base(void)
{
    return SCB->VTOR;
}
/* end of file --------------------------------------------------------------*/
