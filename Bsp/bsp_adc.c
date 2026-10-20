/**
 ******************************************************************************
 *@file               :   bsp_adc.c
 *@brief              :   Calibrated polling ADC without DMA or ADC interrupts.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "adc.h"
#include "bsp_adc.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/
static uint8_t g_adc_initialized;
static uint8_t g_adc_active;

/* private  functions  ------------------------------------------------------*/

/* exported functions -------------------------------------------------------*/
uint8_t bsp_adc_init(void)
{
    g_adc_initialized = 0U;
    if(NULL != hadc1.DMA_Handle ||
       ADC_RESOLUTION_12B != hadc1.Init.Resolution ||
       ADC_DATAALIGN_RIGHT != hadc1.Init.DataAlign ||
       ADC_SCAN_DISABLE != hadc1.Init.ScanConvMode ||
       ADC_SOFTWARE_START != hadc1.Init.ExternalTrigConv ||
       ADC_EOC_SINGLE_CONV != hadc1.Init.EOCSelection ||
       1U != hadc1.Init.NbrOfConversion)
    {
        return 0U;
    }
    if(0U == bsp_adc_stop())
    {
        return 0U;
    }
    /* Override the generated continuous setting only at the BSP boundary. */
    hadc1.Init.ContinuousConvMode = DISABLE;
    if(HAL_OK != HAL_ADC_Init(&hadc1) ||
       HAL_OK != HAL_ADCEx_Calibration_Start(&hadc1))
    {
        return 0U;
    }
    g_adc_initialized = 1U;
    return 1U;
}
uint8_t bsp_adc_start(void)
{
    if(0U == g_adc_initialized || 0U != g_adc_active)
    {
        return 0U;
    }
    if(HAL_OK != HAL_ADC_Start(&hadc1))
    {
        return 0U;
    }
    g_adc_active = 1U;
    return 1U;
}
bsp_adc_result_t bsp_adc_read(uint16_t *p_raw)
{
    uint32_t raw;
    if(NULL == p_raw || 0U == g_adc_active)
    {
        return BSP_ADC_ERROR;
    }
    if(0U != __HAL_ADC_GET_FLAG(&hadc1, ADC_FLAG_OVR))
    {
        return BSP_ADC_ERROR;
    }
    if(0U == __HAL_ADC_GET_FLAG(&hadc1, ADC_FLAG_EOC))
    {
        return BSP_ADC_PENDING;
    }
    /* EOC is already present, so HAL's timeout-zero call cannot block. */
    if(HAL_OK != HAL_ADC_PollForConversion(&hadc1, 0U))
    {
        return BSP_ADC_ERROR;
    }
    raw = HAL_ADC_GetValue(&hadc1);
    if(0U == bsp_adc_stop() || raw > 4095U)
    {
        return BSP_ADC_ERROR;
    }
    *p_raw = (uint16_t)raw;
    return BSP_ADC_READY;
}
uint8_t bsp_adc_stop(void)
{
    if(HAL_OK != HAL_ADC_Stop(&hadc1))
    {
        return 0U;
    }
    g_adc_active = 0U;
    return 1U;
}

/* end of file --------------------------------------------------------------*/
