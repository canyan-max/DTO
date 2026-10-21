/**
 ******************************************************************************
 *@file               :   app_main.c
 *@brief              :   Application assembly and optional DTO routing.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "app_main.h"
#include "app_dto.h"
#include "app_local.h"
#include "bsp_io.h"
#include "bsp_system.h"
#include "bsp_uart.h"
#include "svc_gm800.h"
#include "svc_params.h"
#include "svc_boot.h"
#include "svc_sampling.h"
#include "svc_serial.h"
#include "svc_relay.h"
#include "dto_config.h"
#include "app_hik.h"
#include "svc_telemetry.h"
#include "svc_identity.h"
#if COMM_APP_ENABLE_MODE
#include "app_mode.h"
#endif
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/
static uint32_t g_led_heartbeat_ms;
static uint32_t g_led_activity_ms[2U];
static uint32_t g_led_rx_bytes[2U];
static uint8_t  g_led_state[3U];
static uint8_t  g_led_heartbeat;
static uint8_t  g_led_activity[2U];
static uint8_t  g_outputs_prepared;

/* private  functions  ------------------------------------------------------*/
static uint8_t update_indicators(void)
{
    uint32_t           now_ms = bsp_system_time_ms();
    uint32_t           index;
    uint8_t            levels[3U];
    svc_serial_stats_t stats;
    svc_relay_status_t relay;
    if(0U == svc_relay_get_status(&relay))
    {
        return 0U;
    }
    if((uint32_t)(now_ms - g_led_heartbeat_ms) >= DTO_LED_HEARTBEAT_MS)
    {
        g_led_heartbeat_ms = now_ms;
        g_led_heartbeat    = 0U == g_led_heartbeat ? 1U : 0U;
    }
    levels[0U] = g_led_heartbeat;
    for(index = 0U; index < 2U; index++)
    {
        if(0U == svc_serial_get_stats((comm_port_t)(index + 1U), &stats))
        {
            return 0U;
        }
        if(stats.rx_bytes != g_led_rx_bytes[index])
        {
            g_led_rx_bytes[index]    = stats.rx_bytes;
            g_led_activity_ms[index] = now_ms;
            g_led_activity[index]    = 1U;
        }
        if((uint32_t)(now_ms - g_led_activity_ms[index]) >= DTO_LED_ACTIVITY_MS)
        {
            g_led_activity[index] = 0U;
        }
        levels[index + 1U] = g_led_activity[index];
    }
    for(index = 0U; index < 3U; index++)
    {
        uint8_t level = 0U != relay.uv_active ? 0U : levels[index];
        if(g_led_state[index] != level)
        {
            if(0U == bsp_io_set_led((uint8_t)index, level))
            {
                return 0U;
            }
            g_led_state[index] = level;
        }
    }
    return 1U;
}

/* exported functions -------------------------------------------------------*/
uint8_t app_prepare_outputs(void)
{
    uint8_t gpio_mask;
    if(0U != g_outputs_prepared)
    {
        return 1U;
    }
    svc_params_init();
    if(0U == svc_relay_prepare_startup(&gpio_mask) ||
       0U == bsp_io_preload_relays(gpio_mask))
    {
        return 0U;
    }
    g_outputs_prepared = 1U;
    return 1U;
}
uint8_t app_init(void)
{
    comm_params_t params;
    if(0U == app_prepare_outputs())
    {
        return 0U;
    }
    svc_boot_init();
    svc_identity_init();
    if(0U == svc_relay_init())
    {
        return 0U;
    }
    if(0U == svc_params_get(&params) ||
       0U == bsp_uart_startup_baud(params.rs485_1_baud, params.rs485_2_baud))
    {
        return 0U;
    }
    /* Same power enables as Program; relay levels retain the startup snapshot.
     */
    if(0U == bsp_io_set_power(1U, 1U, 1U))
    {
        return 0U;
    }
    if(0U == svc_serial_init())
    {
        return 0U;
    }
    svc_sampling_init();
#if COMM_APP_ENABLE_MODE
    if(0U == app_mode_init((comm_app_mode_t)params.app_mode))
    {
        return 0U;
    }
#elif COMM_APP_ENABLE_HIK
    svc_telemetry_init();
    app_hik_init();
#endif
    g_led_heartbeat_ms = bsp_system_time_ms();
    {
        uint8_t index;
        for(index = 0U; index < 3U; index++)
        {
            if(0U == bsp_io_set_led(index, 0U))
            {
                return 0U;
            }
        }
    }
    return 1U;
}
uint8_t app_task(void)
{
    svc_serial_task();
    svc_sampling_task();
    if(0U == svc_relay_task() || 0U == update_indicators())
    {
        return 0U;
    }
    if(0U == app_local_task())
    {
        return 0U;
    }
    if(0U == svc_gm800_task())
    {
        return 0U;
    }
#if COMM_APP_ENABLE_MODE
    if(0U == app_mode_task())
    {
        return 0U;
    }
#else
#if COMM_APP_ENABLE_DTO
    if(0U == app_dto_task())
    {
        return 0U;
    }
#endif
#if COMM_APP_ENABLE_HIK
    if(0U == app_hik_task())
    {
        return 0U;
    }
#endif
#endif
    return bsp_system_feed_watchdog();
}

/* end of file --------------------------------------------------------------*/
