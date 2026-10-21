/**
 ******************************************************************************
 *@file               :   svc_xdz_poll.c
 *@brief              :   Nonblocking UART2 XDZ battery acquisition.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include <string.h>
#include "svc_xdz_poll.h"
#include "bsp_system.h"
#include "svc_serial.h"
#include "svc_telemetry.h"
#include "poll_config.h"
#include "xdz_config.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/
svc_xdz_poll_status_t   g_xdz_poll;
static proto_xdz_data_t g_partial;
static proto_xdz_data_t g_snapshot;
static uint32_t         g_part_ms[2U];
static uint32_t         g_updated_ms;
static uint32_t         g_started_ms;
static uint32_t         g_wait_ms;
static uint32_t         g_quiet_ms;
static uint32_t         g_quiet_limit_ms;
static uint32_t         g_rx_bytes;
static uint32_t         g_tx_errors;
static uint8_t          g_valid;

/* private  functions  ------------------------------------------------------*/

static void quiet(uint32_t duration_ms)
{
    g_xdz_poll.state = SVC_XDZ_POLL_QUIET;
    g_quiet_ms       = bsp_system_time_ms();
    g_quiet_limit_ms = duration_ms;
}
static void finish(uint8_t success)
{
    if(0U == success)
    {
        g_xdz_poll.parts = 0U;
    }
    g_xdz_poll.step ^= 1U;
    quiet(0U != success ? COMM_UART2_RX_IDLE_MS : POLL_RETRY_QUIET_MS);
}
static uint16_t capacity_10mah(uint16_t deci_ah)
{
    uint32_t result = (uint32_t)deci_ah * 10U;
    return result > 65535U ? 65535U : (uint16_t)result;
}
static uint8_t publish(void)
{
    comm_battery_data_t battery;
    uint8_t             index;
    uint32_t            now_ms = bsp_system_time_ms();
    g_part_ms[g_xdz_poll.step] = now_ms;
    g_xdz_poll.parts |= (uint8_t)(1U << g_xdz_poll.step);
    for(index = 0U; index < 2U; index++)
    {
        if((uint32_t)(now_ms - g_part_ms[index]) >= XDZ_CACHE_STALE_MS)
        {
            g_xdz_poll.parts &= (uint8_t)~(1U << index);
        }
    }
    if(3U != g_xdz_poll.parts)
    {
        return 1U;
    }
    memset(&battery, 0, sizeof(battery));
    battery.voltage_10mv    = g_partial.voltage_10mv;
    battery.current_10ma    = g_partial.current_10ma;
    battery.remaining_10mah = capacity_10mah(g_partial.remaining_deci_ah);
    battery.nominal_10mah   = capacity_10mah(g_partial.nominal_deci_ah);
    battery.soc_percent     = g_partial.soc_percent;
    battery.fet_state       = g_partial.fet_state;
    battery.protection      = g_partial.protection;
    battery.ntc_count       = 2U;
    battery.temperature_deci_kelvin[0U] = g_partial.temperature_deci_kelvin[0U];
    battery.temperature_deci_kelvin[1U] = g_partial.temperature_deci_kelvin[1U];
    if(0U == svc_telemetry_publish_battery(&battery))
    {
        return 0U;
    }
    g_snapshot   = g_partial;
    g_valid      = 1U;
    g_updated_ms = now_ms;
    g_xdz_poll.updates++;
    return 1U;
}

/* exported functions -------------------------------------------------------*/

void svc_xdz_poll_init(void)
{
    svc_serial_stats_t stats;
    memset(&g_xdz_poll, 0, sizeof(g_xdz_poll));
    memset(&g_partial, 0, sizeof(g_partial));
    memset(&g_snapshot, 0, sizeof(g_snapshot));
    memset(g_part_ms, 0, sizeof(g_part_ms));
    g_valid      = 0U;
    g_rx_bytes   = 0U;
    g_tx_errors  = 0U;
    g_started_ms = bsp_system_time_ms();
    if(0U != svc_serial_get_stats(XDZ_DEVICE_PORT, &stats))
    {
        g_rx_bytes  = stats.rx_bytes;
        g_tx_errors = stats.tx_errors;
    }
    quiet(COMM_UART2_RX_IDLE_MS);
}
void svc_xdz_poll_pause(void)
{
    g_xdz_poll.state = SVC_XDZ_POLL_PAUSED;
    g_xdz_poll.parts = 0U;
    g_valid          = 0U;
    svc_telemetry_invalidate();
}
uint8_t svc_xdz_poll_get(proto_xdz_data_t *p_data, uint8_t *p_valid)
{
    if(NULL == p_data || NULL == p_valid)
    {
        return 0U;
    }
    if((uint32_t)(bsp_system_time_ms() - g_updated_ms) >= XDZ_CACHE_STALE_MS)
    {
        g_valid = 0U;
    }
    *p_valid = g_valid;
    if(0U != g_valid)
    {
        *p_data = g_snapshot;
    }
    else
    {
        memset(p_data, 0, sizeof(*p_data));
        /* Zero Celsius on the legacy host, not fictitious -273 C. */
        p_data->temperature_deci_kelvin[0U] = 2731U;
        p_data->temperature_deci_kelvin[1U] = 2731U;
    }
    return 1U;
}
uint8_t svc_xdz_poll_task(void)
{
    uint32_t            now_ms = bsp_system_time_ms();
    svc_serial_stats_t  stats;
    uint16_t            pending;
    uint8_t             frame[8U];
    svc_serial_result_t result;
    if(0U == svc_serial_get_stats(XDZ_DEVICE_PORT, &stats) ||
       0U == svc_serial_tx_pending(XDZ_DEVICE_PORT, &pending))
    {
        return 0U;
    }
    if(SVC_XDZ_POLL_PAUSED == g_xdz_poll.state)
    {
        g_xdz_poll.step = 0U;
        g_started_ms    = now_ms;
        g_rx_bytes      = stats.rx_bytes;
        quiet(COMM_UART2_RX_IDLE_MS);
        return svc_serial_discard_rx(XDZ_DEVICE_PORT);
    }
    if(stats.rx_bytes != g_rx_bytes)
    {
        g_rx_bytes = stats.rx_bytes;
        g_quiet_ms = now_ms;
    }
    if(SVC_XDZ_POLL_QUEUED == g_xdz_poll.state ||
       SVC_XDZ_POLL_WAITING == g_xdz_poll.state)
    {
        if(stats.tx_errors != g_tx_errors)
        {
            g_xdz_poll.tx_failures++;
            finish(0U);
            return 1U;
        }
        if(SVC_XDZ_POLL_QUEUED == g_xdz_poll.state)
        {
            if(0U == pending)
            {
                g_xdz_poll.state = SVC_XDZ_POLL_WAITING;
                g_wait_ms        = now_ms;
            }
            else if((uint32_t)(now_ms - g_started_ms) >= POLL_QUEUE_MS)
            {
                g_xdz_poll.tx_failures++;
                finish(0U);
                return 1U;
            }
        }
        if(SVC_XDZ_POLL_WAITING == g_xdz_poll.state &&
           (uint32_t)(now_ms - g_wait_ms) >= POLL_RESPONSE_MS)
        {
            g_xdz_poll.timeouts++;
            finish(0U);
        }
        return 1U;
    }
    if(0U != pending || (uint32_t)(now_ms - g_quiet_ms) < g_quiet_limit_ms ||
       (uint32_t)(now_ms - g_started_ms) < POLL_INTERVAL_MS)
    {
        return 1U;
    }
    if(8U != proto_xdz_query(g_xdz_poll.step, frame, sizeof(frame)))
    {
        return 0U;
    }
    result = svc_serial_send(XDZ_DEVICE_PORT, frame, sizeof(frame));
    if(SVC_SERIAL_FULL == result)
    {
        return 1U;
    }
    if(SVC_SERIAL_OK != result)
    {
        return 0U;
    }
    g_started_ms     = now_ms;
    g_tx_errors      = stats.tx_errors;
    g_xdz_poll.state = SVC_XDZ_POLL_QUEUED;
    g_xdz_poll.sent++;
    return 1U;
}
void svc_xdz_poll_accept(const uint8_t *p_data, uint16_t length)
{
    svc_serial_stats_t stats;
    uint16_t           pending;
    uint32_t           now_ms = bsp_system_time_ms();
    proto_xdz_result_t result;
    if(NULL == p_data ||
       (SVC_XDZ_POLL_QUEUED != g_xdz_poll.state &&
        SVC_XDZ_POLL_WAITING != g_xdz_poll.state) ||
       0U == svc_serial_get_stats(XDZ_DEVICE_PORT, &stats) ||
       0U == svc_serial_tx_pending(XDZ_DEVICE_PORT, &pending) ||
       0U != pending || stats.tx_errors != g_tx_errors ||
       (SVC_XDZ_POLL_QUEUED == g_xdz_poll.state &&
        (uint32_t)(now_ms - g_started_ms) >= POLL_QUEUE_MS) ||
       (SVC_XDZ_POLL_WAITING == g_xdz_poll.state &&
        (uint32_t)(now_ms - g_wait_ms) >= POLL_RESPONSE_MS))
    {
        g_xdz_poll.invalid++;
        return;
    }
    result = proto_xdz_decode(g_xdz_poll.step, p_data, length, &g_partial);
    if(PROTO_XDZ_EXCEPTION == result)
    {
        g_xdz_poll.exceptions++;
        finish(0U);
    }
    else if(PROTO_XDZ_DATA == result)
    {
        if(0U == publish())
        {
            g_xdz_poll.invalid++;
            return;
        }
        g_xdz_poll.received++;
        finish(1U);
    }
    else
    {
        g_xdz_poll.invalid++;
    }
}

/* end of file --------------------------------------------------------------*/
