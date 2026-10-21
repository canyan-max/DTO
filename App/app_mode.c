/**
 ******************************************************************************
 *@file               :   app_mode.c
 *@brief              :   Transfer serial ownership safely across DTO, HIK and XDZ.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "app_mode.h"
#include "app_dto.h"
#include "app_hik.h"
#include "app_xdz.h"
#include "app_local.h"
#include "bsp_system.h"
#include "svc_gm800.h"
#include "svc_params.h"
#include "svc_serial.h"
#include "svc_telemetry.h"

/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/
app_mode_status_t g_mode;
static uint32_t   g_started_ms;
static uint32_t   g_quiet_ms;
static uint32_t   g_rx_bytes[COMM_PORT_NUM];
static uint8_t    g_failed;

/* private  functions  ------------------------------------------------------*/
static uint8_t reset_rx(void)
{
    uint32_t           index;
    svc_serial_stats_t stats;
    for(index = 0U; index < COMM_PORT_NUM; index++)
    {
        if(0U == svc_serial_discard_rx((comm_port_t)index) ||
           0U == svc_serial_get_stats((comm_port_t)index, &stats))
        {
            return 0U;
        }
        g_rx_bytes[index] = stats.rx_bytes;
    }
    g_quiet_ms = bsp_system_time_ms();
    return 1U;
}
static void prepare_mode(comm_app_mode_t mode)
{
    app_dto_discard_pending();
    app_hik_discard_pending();
    app_xdz_discard_pending();
    svc_telemetry_init();
    if(COMM_APP_MODE_HIK == mode)
    {
        app_hik_init();
    }
    else if(COMM_APP_MODE_XDZ == mode)
    {
        app_xdz_init();
    }
}

/* exported functions -------------------------------------------------------*/
uint8_t app_mode_init(comm_app_mode_t initial)
{
    if((uint32_t)initial >= COMM_APP_MODE_NUM)
    {
        return 0U;
    }
    g_mode.active    = initial;
    g_mode.requested = initial;
    g_mode.state     = APP_MODE_ACTIVE;
    g_mode.switches  = 0U;
    g_mode.failures  = 0U;
    g_failed         = 0U;
    prepare_mode(initial);
    return 1U;
}
uint8_t app_mode_request(comm_app_mode_t mode)
{
    if((uint32_t)mode >= COMM_APP_MODE_NUM || APP_MODE_ACTIVE != g_mode.state ||
       0U != svc_gm800_busy())
    {
        return 0U;
    }
    if(mode == g_mode.active)
    {
        return 1U;
    }
    g_mode.requested = mode;
    g_mode.state     = APP_MODE_DRAINING;
    g_started_ms     = bsp_system_time_ms();
    g_failed         = 0U;
    /* Drop unsent application frames; serial TX already queued is drained. */
    app_dto_discard_pending();
    app_hik_discard_pending();
    app_xdz_discard_pending();
    svc_telemetry_invalidate();
    return 1U;
}
uint8_t app_mode_task(void)
{
    uint32_t           index;
    uint32_t           now_ms = bsp_system_time_ms();
    uint16_t           pending;
    svc_serial_stats_t stats;
    comm_params_t      params;
    if(APP_MODE_ACTIVE == g_mode.state)
    {
        if(COMM_APP_MODE_DTO == g_mode.active)
        {
            return app_dto_task();
        }
        return COMM_APP_MODE_HIK == g_mode.active ? app_hik_task()
                                                  : app_xdz_task();
    }
    if(0U == g_failed &&
       (uint32_t)(now_ms - g_started_ms) >= COMM_MODE_SWITCH_TIMEOUT_MS)
    {
        /* Cancel the target. Do not start either RX owner while old TX or
         * continuous incoming traffic still prevents a clean boundary. */
        g_mode.failures++;
        g_failed         = 1U;
        g_mode.requested = g_mode.active;
        g_mode.state     = APP_MODE_DRAINING;
    }
    if(0U != svc_gm800_busy() || 0U != app_local_busy())
    {
        return 1U;
    }
    for(index = 0U; index < COMM_PORT_NUM; index++)
    {
        if(0U == svc_serial_tx_pending((comm_port_t)index, &pending))
        {
            return 0U;
        }
        if(0U != pending)
        {
            return 1U;
        }
    }
    if(APP_MODE_DRAINING == g_mode.state)
    {
        if(0U == reset_rx())
        {
            return 0U;
        }
        g_mode.state = APP_MODE_QUIET;
        return 1U;
    }
    for(index = 0U; index < COMM_PORT_NUM; index++)
    {
        if(0U == svc_serial_get_stats((comm_port_t)index, &stats))
        {
            return 0U;
        }
        if(stats.rx_bytes != g_rx_bytes[index])
        {
            return reset_rx();
        }
    }
    if((uint32_t)(now_ms - g_quiet_ms) < COMM_MODE_SWITCH_QUIET_MS)
    {
        return 1U;
    }
    if(0U == svc_params_get(&params))
    {
        return 0U;
    }
    params.app_mode = (uint8_t)g_mode.requested;
    if(0U == svc_params_set(&params))
    {
        return 0U;
    }
    prepare_mode(g_mode.requested);
    if(g_mode.active != g_mode.requested)
    {
        g_mode.switches++;
    }
    g_mode.active = g_mode.requested;
    g_mode.state  = APP_MODE_ACTIVE;
    return 1U;
}

/* end of file --------------------------------------------------------------*/
