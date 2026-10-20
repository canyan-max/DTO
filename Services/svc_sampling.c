/**
 ******************************************************************************
 *@file               :   svc_sampling.c
 *@brief              :   Cooperative ADC schedule and independent block average.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include <string.h>
#include "svc_sampling.h"
#include "bsp_adc.h"
#include "bsp_system.h"
#include "sampling_config.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/
 svc_sampling_status_t g_sampling;
static uint32_t              g_sum;
static uint16_t              g_count;
static uint32_t              g_attempt_ms;
static uint32_t              g_started_ms;

/* private  functions  ------------------------------------------------------*/
static void invalidate(void)
{
    g_sampling.valid = 0U;
    g_sum            = 0U;
    g_count          = 0U;
}
static void recover_later(uint32_t now_ms)
{
    g_sampling.errors++;
    g_sampling.initialized = 0U;
    /* Keep active visible if HAL failed to stop; init retries stop first. */
    if(0U != bsp_adc_stop())
    {
        g_sampling.active = 0U;
    }
    invalidate();
    g_attempt_ms = now_ms;
}
static void try_init(uint32_t now_ms)
{
    g_attempt_ms = now_ms;
    if(0U == bsp_adc_init())
    {
        g_sampling.errors++;
        return;
    }
    g_sampling.initialized = 1U;
    g_sampling.active      = 0U;
}

/* exported functions -------------------------------------------------------*/
void svc_sampling_init(void)
{
    memset(&g_sampling, 0, sizeof(g_sampling));
    invalidate();
    try_init(bsp_system_time_ms());
}
void svc_sampling_task(void)
{
    uint32_t         now_ms = bsp_system_time_ms();
    uint16_t         raw;
    bsp_adc_result_t result;
    if((0U != g_sampling.valid || 0U != g_count) &&
       (uint32_t)(now_ms - g_sampling.last_sample_ms) >= SAMPLING_STALE_MS)
    {
        invalidate();
    }
    if(0U == g_sampling.initialized)
    {
        if((uint32_t)(now_ms - g_attempt_ms) >= SAMPLING_RETRY_MS)
        {
            try_init(now_ms);
        }
        return;
    }
    if(0U == g_sampling.active)
    {
        if((uint32_t)(now_ms - g_attempt_ms) < SAMPLING_PERIOD_MS)
        {
            return;
        }
        g_attempt_ms = now_ms;
        if(0U == bsp_adc_start())
        {
            recover_later(now_ms);
            return;
        }
        g_started_ms      = now_ms;
        g_sampling.active = 1U;
        return;
    }
    result = bsp_adc_read(&raw);
    if(BSP_ADC_PENDING == result)
    {
        if((uint32_t)(now_ms - g_started_ms) >= SAMPLING_TIMEOUT_MS)
        {
            g_sampling.timeouts++;
            recover_later(now_ms);
        }
        return;
    }
    if(BSP_ADC_READY != result)
    {
        recover_later(now_ms);
        return;
    }
    g_sampling.active         = 0U;
    g_sampling.raw            = raw;
    g_sampling.last_sample_ms = now_ms;
    g_sampling.samples++;
    g_sum += raw;
    g_count++;
    if(SAMPLING_FILTER_COUNT == g_count)
    {
        g_sampling.filtered_raw = (uint16_t)(g_sum / SAMPLING_FILTER_COUNT);
        /* Preserve Program's two integer divisions, including truncation. */
        g_sampling.adc_pin_mv = (uint16_t)((uint32_t)g_sampling.filtered_raw *
                                           SAMPLING_VREF_MV /
                                           SAMPLING_ADC_FULL_SCALE);
        g_sampling.input_mv   = (uint16_t)((uint32_t)g_sampling.adc_pin_mv *
                                           SAMPLING_DIVIDER_NUMERATOR /
                                           SAMPLING_DIVIDER_DENOMINATOR);
        g_sampling.last_update_ms = now_ms;
        g_sampling.updates++;
        g_sampling.valid = 1U;
        g_sum            = 0U;
        g_count          = 0U;
    }
}
uint8_t svc_sampling_get_status(svc_sampling_status_t *p_status)
{
    if(NULL == p_status)
    {
        return 0U;
    }
    *p_status = g_sampling;
    if((uint32_t)(bsp_system_time_ms() - g_sampling.last_sample_ms) >=
       SAMPLING_STALE_MS)
    {
        p_status->valid = 0U;
    }
    return 1U;
}

/* end of file --------------------------------------------------------------*/
