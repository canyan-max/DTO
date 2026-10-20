/**
 ******************************************************************************
 *@file               :   svc_poll.c
 *@brief              :   Nonblocking legacy controller detection and polling.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include <string.h>
#include "svc_poll.h"
#include "bsp_system.h"
#include "hik_config.h"
#include "poll_config.h"
#include "svc_serial.h"
#include "svc_telemetry.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/
svc_poll_status_t          g_poll;
static comm_battery_data_t g_battery;
static comm_solar_data_t   g_solar;
static uint32_t            g_part_ms[4U];
static uint32_t            g_started_ms;
static uint32_t            g_wait_ms;
static uint32_t            g_good_ms;
static uint32_t            g_quiet_ms;
static uint32_t            g_quiet_limit_ms;
static uint32_t            g_rx_bytes;
static uint32_t            g_tx_errors;

/* private  functions  ------------------------------------------------------*/
static void clear_partial(void)
{
    memset(&g_battery, 0, sizeof(g_battery));
    memset(&g_solar, 0, sizeof(g_solar));
    memset(g_part_ms, 0, sizeof(g_part_ms));
    g_poll.battery_parts = 0U;
}
static void quiet(uint32_t limit_ms)
{
    g_poll.state     = SVC_POLL_QUIET;
    g_quiet_ms       = bsp_system_time_ms();
    g_quiet_limit_ms = limit_ms;
}
static uint8_t finish(uint8_t success)
{
    if(0U == success)
    {
        g_poll.battery_parts = 0U;
    }
    g_poll.step = proto_controller_next(g_poll.profile, g_poll.step);
    quiet(0U != success ? COMM_UART3_RX_IDLE_MS : POLL_RETRY_QUIET_MS);
    /* Preserve subsequent local frames on the shared port for App. Late
     * controller replies are rejected while QUIET; the quiet guard postpones
     * the next controller request without erasing completed local commands. */
    return 1U;
}
static uint8_t publish(uint32_t now_ms)
{
    uint8_t part = 0U;
    uint8_t i;
    if(0U == g_poll.step)
    {
        return 1U;
    }
    if((PROTO_CONTROLLER_EVE == g_poll.profile && 1U == g_poll.step) ||
       (PROTO_CONTROLLER_SZS == g_poll.profile && 1U == g_poll.step))
    {
        if(0U == svc_telemetry_publish_solar(&g_solar))
        {
            return 0U;
        }
        g_poll.solar_updates++;
    }
    if(PROTO_CONTROLLER_EVE == g_poll.profile && g_poll.step >= 2U)
    {
        part            = g_poll.step - 2U;
        g_part_ms[part] = now_ms;
        g_poll.battery_parts |= (uint8_t)(1U << part);
        /* A successful solar reply must not refresh old battery groups. */
        for(i = 0U; i < 4U; i++)
        {
            if((uint32_t)(now_ms - g_part_ms[i]) >= HIK_CACHE_STALE_MS)
            {
                g_poll.battery_parts &= (uint8_t)~(1U << i);
            }
        }
        if(0x0FU != g_poll.battery_parts)
        {
            return 1U;
        }
    }
    else if(PROTO_CONTROLLER_SZS != g_poll.profile || 1U != g_poll.step)
    {
        return 1U;
    }
    /* Controllers provide capacity and SOC, not measured remaining charge.
     * Keep 0.01 Ah units and widen before multiplication; truncate below it. */
    g_battery.remaining_10mah = (uint16_t)((uint32_t)g_battery.nominal_10mah *
                                           g_battery.soc_percent / 100U);
    if(0U == svc_telemetry_publish_battery(&g_battery))
    {
        return 0U;
    }
    g_poll.battery_updates++;
    return 1U;
}

/* exported functions -------------------------------------------------------*/
void svc_poll_init(void)
{
    svc_serial_stats_t stats;
    memset(&g_poll, 0, sizeof(g_poll));
    clear_partial();
    g_started_ms = bsp_system_time_ms();
    g_good_ms    = g_started_ms;
    g_rx_bytes   = 0U;
    g_tx_errors  = 0U;
    if(0U != svc_serial_get_stats(HIK_DEVICE_PORT, &stats))
    {
        g_rx_bytes  = stats.rx_bytes;
        g_tx_errors = stats.tx_errors;
    }
    quiet(COMM_UART3_RX_IDLE_MS);
}
void svc_poll_pause(void)
{
    if(SVC_POLL_PAUSED != g_poll.state)
    {
        g_poll.state = SVC_POLL_PAUSED;
        clear_partial();
        svc_telemetry_invalidate();
    }
}
uint8_t svc_poll_task(void)
{
    uint32_t            now_ms = bsp_system_time_ms();
    svc_serial_stats_t  stats;
    uint16_t            pending;
    uint8_t             frame[8U];
    svc_serial_result_t result;
    if(0U == svc_serial_get_stats(HIK_DEVICE_PORT, &stats) ||
       0U == svc_serial_tx_pending(HIK_DEVICE_PORT, &pending))
    {
        return 0U;
    }
    if(SVC_POLL_PAUSED == g_poll.state)
    {
        g_poll.step  = 0U;
        g_good_ms    = now_ms;
        g_started_ms = now_ms;
        g_rx_bytes   = stats.rx_bytes;
        quiet(COMM_UART3_RX_IDLE_MS);
        return svc_serial_discard_rx(HIK_DEVICE_PORT);
    }
    if(g_rx_bytes != stats.rx_bytes)
    {
        g_rx_bytes = stats.rx_bytes;
        g_quiet_ms = now_ms;
    }
    if(SVC_POLL_QUEUED == g_poll.state || SVC_POLL_WAITING == g_poll.state)
    {
        if(stats.tx_errors != g_tx_errors)
        {
            g_poll.tx_failures++;
            return finish(0U);
        }
        if(SVC_POLL_QUEUED == g_poll.state)
        {
            if(0U == pending)
            {
                g_poll.state = SVC_POLL_WAITING;
                g_wait_ms    = now_ms;
            }
            else if((uint32_t)(now_ms - g_started_ms) >= POLL_QUEUE_MS)
            {
                g_poll.tx_failures++;
                return finish(0U);
            }
        }
        if(SVC_POLL_WAITING == g_poll.state &&
           (uint32_t)(now_ms - g_wait_ms) >= POLL_RESPONSE_MS)
        {
            g_poll.timeouts++;
            return finish(0U);
        }
        return 1U;
    }
    if(0U != pending)
    {
        return 1U;
    }
    if((uint32_t)(now_ms - g_good_ms) >= POLL_DETECT_MS)
    {
        g_poll.profile = PROTO_CONTROLLER_EVE == g_poll.profile
                             ? PROTO_CONTROLLER_SZS
                             : PROTO_CONTROLLER_EVE;
        g_poll.switches++;
        g_poll.step = 0U;
        g_good_ms   = now_ms;
        clear_partial();
        svc_telemetry_invalidate();
        quiet(POLL_RETRY_QUIET_MS);
        return svc_serial_discard_rx(HIK_DEVICE_PORT);
    }
    if((uint32_t)(now_ms - g_quiet_ms) < g_quiet_limit_ms)
    {
        return 1U;
    }
    g_poll.state = SVC_POLL_READY;
    if((uint32_t)(now_ms - g_started_ms) < POLL_INTERVAL_MS)
    {
        return 1U;
    }
    if(8U != proto_controller_query(g_poll.profile, g_poll.step, frame,
                                    sizeof(frame)))
    {
        return 0U;
    }
    result = svc_serial_send(HIK_DEVICE_PORT, frame, sizeof(frame));
    if(SVC_SERIAL_FULL == result)
    {
        return 1U;
    }
    if(SVC_SERIAL_OK != result)
    {
        return 0U;
    }
    g_started_ms = now_ms;
    g_tx_errors  = stats.tx_errors;
    g_poll.state = SVC_POLL_QUEUED;
    g_poll.sent++;
    return 1U;
}
void svc_poll_accept(const uint8_t *p_data, uint16_t length)
{
    uint32_t                  now_ms = bsp_system_time_ms();
    svc_serial_stats_t        stats;
    uint16_t                  pending;
    proto_controller_result_t result;
    if(NULL == p_data ||
       (SVC_POLL_QUEUED != g_poll.state && SVC_POLL_WAITING != g_poll.state))
    {
        g_poll.invalid++;
        return;
    }
    if(0U == svc_serial_get_stats(HIK_DEVICE_PORT, &stats) ||
       0U == svc_serial_tx_pending(HIK_DEVICE_PORT, &pending) ||
       0U != pending || stats.tx_errors != g_tx_errors ||
       (SVC_POLL_QUEUED == g_poll.state &&
        (uint32_t)(now_ms - g_started_ms) >= POLL_QUEUE_MS) ||
       (SVC_POLL_WAITING == g_poll.state &&
        (uint32_t)(now_ms - g_wait_ms) >= POLL_RESPONSE_MS))
    {
        g_poll.invalid++;
        return;
    }
    result = proto_controller_decode(g_poll.profile, g_poll.step, p_data,
                                     length, &g_battery, &g_solar);
    if(PROTO_CONTROLLER_EXCEPTION == result)
    {
        g_poll.exceptions++;
        /* Exceptions are not evidence that measurements were acquired. */
        if(0U == finish(0U))
        {
            svc_poll_pause();
        }
    }
    else if(PROTO_CONTROLLER_DATA == result)
    {
        if(0U == publish(now_ms))
        {
            g_poll.invalid++;
            return;
        }
        g_poll.received++;
        g_good_ms = now_ms;
        if(0U == finish(1U))
        {
            svc_poll_pause();
        }
    }
    else
    {
        g_poll.invalid++;
    }
}

/* end of file --------------------------------------------------------------*/
