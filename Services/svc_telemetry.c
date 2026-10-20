/**
 ******************************************************************************
 *@file               :   svc_telemetry.c
 *@brief              :   Validate and age external measurements without HAL.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include <string.h>
#include "svc_telemetry.h"
#include "bsp_system.h"
#include "hik_config.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/
svc_telemetry_snapshot_t g_telemetry;

/* private  functions  ------------------------------------------------------*/

/* exported functions -------------------------------------------------------*/
void svc_telemetry_init(void)
{
    memset(&g_telemetry, 0, sizeof(g_telemetry));
}
uint8_t svc_telemetry_publish_battery(const comm_battery_data_t *p_data)
{
    if(NULL == p_data || p_data->ntc_count > COMM_TELEMETRY_NTC_MAX ||
       p_data->soc_percent > 100U || p_data->fet_state > 3U)
    {
        return 0U;
    }
    g_telemetry.battery            = *p_data;
    g_telemetry.battery_updated_ms = bsp_system_time_ms();
    g_telemetry.battery_valid      = 1U;
    return 1U;
}
uint8_t svc_telemetry_publish_solar(const comm_solar_data_t *p_data)
{
    if(NULL == p_data)
    {
        return 0U;
    }
    g_telemetry.solar            = *p_data;
    g_telemetry.solar_updated_ms = bsp_system_time_ms();
    g_telemetry.solar_valid      = 1U;
    return 1U;
}
uint8_t svc_telemetry_get(svc_telemetry_snapshot_t *p_snapshot)
{
    uint32_t now_ms = bsp_system_time_ms();
    if(NULL == p_snapshot)
    {
        return 0U;
    }
    /* Latch expiry until a new validated publication, including tick rollover.
     */
    if((uint32_t)(now_ms - g_telemetry.battery_updated_ms) >=
       HIK_CACHE_STALE_MS)
    {
        g_telemetry.battery_valid = 0U;
    }
    if((uint32_t)(now_ms - g_telemetry.solar_updated_ms) >= HIK_CACHE_STALE_MS)
    {
        g_telemetry.solar_valid = 0U;
    }
    *p_snapshot = g_telemetry;
    return 1U;
}
void svc_telemetry_invalidate(void)
{
    g_telemetry.battery_valid = 0U;
    g_telemetry.solar_valid   = 0U;
}

/* end of file --------------------------------------------------------------*/
