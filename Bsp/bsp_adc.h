/**
 ******************************************************************************
 *@file               :   bsp_adc.h
 *@brief              :   Main-context ADC single-conversion access without DMA.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef BSP_ADC_H
#define BSP_ADC_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/
typedef enum BSP_ADC_RESULT_T
{
    BSP_ADC_PENDING = 0U,
    BSP_ADC_READY,
    BSP_ADC_ERROR,
} bsp_adc_result_t;

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
/* After MX_ADC1_Init; selects single conversion and calibrates while stopped.
 */
uint8_t bsp_adc_init(void);
uint8_t bsp_adc_start(void);
/* Checks EOC first; never waits for conversion completion. */
bsp_adc_result_t bsp_adc_read(uint16_t *p_raw);
uint8_t          bsp_adc_stop(void);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* BSP_ADC_H */
/* end of file --------------------------------------------------------------*/
