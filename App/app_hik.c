/**
 ******************************************************************************
 *@file               :   app_hik.c
 *@brief              :   HIK read requests on 485_1 with exclusive RX ownership.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include <string.h>
#include "app_hik.h"
#include "app_local.h"
#include "bsp_system.h"
#include "hik_config.h"
#include "proto_gm800.h"
#include "proto_hik.h"
#include "svc_gm800.h"
#include "svc_serial.h"
#include "svc_telemetry.h"
#include "svc_identity.h"
#include "poll_config.h"
#if COMM_APP_ENABLE_POLL
#include "svc_poll.h"
#endif
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/
static app_hik_status_t g_hik;
static uint8_t          g_rx[COMM_SERIAL_FRAME_SIZE];
static uint8_t          g_control[COMM_SERIAL_FRAME_SIZE];
static uint16_t         g_control_length;
static uint8_t          g_reply[PROTO_HIK_MAX_REPLY];
static uint16_t         g_reply_length;
static uint32_t         g_reply_started_ms;

/* private  functions  ------------------------------------------------------*/
static void discard_reply(void)
{
    if(0U != g_reply_length)
    {
        g_hik.maintenance_dropped++;
        g_reply_length = 0U;
    }
}
static uint8_t flush_reply(void)
{
    svc_serial_result_t result;
    if(0U == g_reply_length)
    {
        return 1U;
    }
    if((uint32_t)(bsp_system_time_ms() - g_reply_started_ms) >=
       HIK_REPLY_QUEUE_TIMEOUT_MS)
    {
        g_hik.queue_timeouts++;
        g_reply_length = 0U;
        return 1U;
    }
    result = svc_serial_send(HIK_HOST_PORT, g_reply, g_reply_length);
    if(SVC_SERIAL_FULL == result)
    {
        g_hik.blocked_attempts++;
        return 1U;
    }
    if(SVC_SERIAL_OK != result)
    {
        return 0U;
    }
    g_hik.replies_queued++;
    g_reply_length = 0U;
    return 1U;
}
/* 0=not a configuration candidate, 1=consumed, 2=started, 3=fatal. */
static uint8_t
configuration(comm_port_t source, const uint8_t *p_data, uint16_t length)
{
    char                  identity[PROTO_GM800_ID_SIZE + 1U];
    proto_gm800_request_t result = proto_gm800_parse_request(p_data, length,
                                                             identity,
                                                             sizeof(identity));
    if(PROTO_GM800_INVALID_REQUEST == result)
    {
        g_hik.invalid_frames++;
        return 1U;
    }
    if(PROTO_GM800_VALID_REQUEST != result)
    {
        return 0U;
    }
    if(0U == svc_gm800_start(source, identity, sizeof(identity)))
    {
        return 3U;
    }
    discard_reply();
    g_control_length = 0U;
    g_hik.configuration_requests++;
    return 2U;
}
static uint8_t local_control(void)
{
    app_local_result_t  local;
    svc_serial_result_t result;
    if(0U == g_control_length)
    {
        result = svc_serial_read(COMM_PORT_4G, g_control, sizeof(g_control),
                                 &g_control_length);
        if(SVC_SERIAL_EMPTY == result)
        {
            return 1U;
        }
        if(SVC_SERIAL_OK != result)
        {
            return 0U;
        }
    }
    local = app_local_accept(COMM_PORT_4G, g_control, g_control_length);
    if(APP_LOCAL_ERROR == local)
    {
        return 0U;
    }
    if(APP_LOCAL_RETRY == local)
    {
        return 1U;
    }
    g_control_length = 0U;
    if(APP_LOCAL_NOT_LOCAL == local)
    {
        g_hik.ignored_frames++;
    }
    if(0U != app_local_maintenance_pending())
    {
        discard_reply();
    }
    return 1U;
}
static uint8_t host_request(uint16_t length)
{
    uint8_t                  command;
    svc_telemetry_snapshot_t snapshot;
    svc_identity_status_t    identity;
    proto_hik_result_t       result = proto_hik_parse(g_rx, length, &command);
    if(PROTO_HIK_INVALID == result)
    {
        g_hik.invalid_frames++;
        return 1U;
    }
    if(PROTO_HIK_UNSUPPORTED == result)
    {
        g_hik.unsupported_frames++;
        return 1U;
    }
    if(PROTO_HIK_READ != result && PROTO_HIK_WRITE_SN != result)
    {
        g_hik.ignored_frames++;
        return 1U;
    }
    g_hik.requests++;
    if(PROTO_HIK_WRITE_SN == result)
    {
        /* Parser already checked payload length, checksum and end marker. */
        if(0U == svc_identity_set_sn(g_rx + 4U, g_rx[3U]))
        {
            return 0U;
        }
        g_reply_length = proto_hik_build_sn_ack(g_reply, sizeof(g_reply));
    }
    else if(PROTO_HIK_SN_READ == command)
    {
        if(0U == svc_identity_get_status(&identity))
        {
            return 0U;
        }
        g_reply_length = proto_hik_build_sn((const uint8_t *)identity.sn,
                                            identity.length, g_reply,
                                            sizeof(g_reply));
    }
    else if(PROTO_HIK_INFO == command)
    {
        g_reply_length = proto_hik_build_info(g_reply, sizeof(g_reply));
    }
    else
    {
        if(0U == svc_telemetry_get(&snapshot))
        {
            return 0U;
        }
        if((PROTO_HIK_BATTERY == command && 0U == snapshot.battery_valid) ||
           (PROTO_HIK_SOLAR == command && 0U == snapshot.solar_valid))
        {
            g_hik.no_data++;
            /* User-selected behavior: reply with zero measurements while
             * keeping the internal cache invalid until actual acquisition. */
            if(PROTO_HIK_BATTERY == command)
            {
                memset(&snapshot.battery, 0, sizeof(snapshot.battery));
                snapshot.battery.software_version = HIK_EMPTY_BATTERY_VERSION;
            }
            else
            {
                memset(&snapshot.solar, 0, sizeof(snapshot.solar));
                snapshot.solar.number = HIK_EMPTY_SOLAR_NUMBER;
            }
        }
        g_reply_length = PROTO_HIK_BATTERY == command
                             ? proto_hik_build_battery(&snapshot.battery,
                                                       g_reply, sizeof(g_reply))
                             : proto_hik_build_solar(&snapshot.solar, g_reply,
                                                     sizeof(g_reply));
    }
    if(0U == g_reply_length)
    {
        return 0U;
    }
    g_reply_started_ms = bsp_system_time_ms();
    return flush_reply();
}

/* exported functions -------------------------------------------------------*/
void app_hik_discard_pending(void)
{
    discard_reply();
    g_control_length = 0U;
#if COMM_APP_ENABLE_POLL
    svc_poll_pause();
#endif
}
void app_hik_init(void)
{
    memset(&g_hik, 0, sizeof(g_hik));
    g_control_length = 0U;
    g_reply_length   = 0U;
#if COMM_APP_ENABLE_POLL
    svc_poll_init();
#endif
}
static uint8_t hik_task(void)
{
    svc_serial_result_t result;
    uint16_t            length;
    uint8_t             config_result;
    app_local_result_t  local;
    if(0U != svc_gm800_busy())
    {
        discard_reply();
        g_control_length = 0U;
        return 1U;
    }
    if(0U != app_local_busy())
    {
        if(0U != app_local_maintenance_pending())
        {
            discard_reply();
        }
        return 1U;
    }
    if(0U == local_control())
    {
        return 0U;
    }
    if(0U != app_local_busy())
    {
        return 1U;
    }
    /* Single RX owner: configuration takes precedence over device replies. */
    result = svc_serial_read(HIK_DEVICE_PORT, g_rx, sizeof(g_rx), &length);
    if(SVC_SERIAL_OK == result)
    {
        local = app_local_accept(HIK_DEVICE_PORT, g_rx, length);
        if(APP_LOCAL_ERROR == local || APP_LOCAL_RETRY == local)
        {
            return 0U;
        }
        if(APP_LOCAL_CONSUMED == local)
        {
            return 1U;
        }
        config_result = configuration(HIK_DEVICE_PORT, g_rx, length);
        if(3U == config_result)
        {
            return 0U;
        }
        if(2U == config_result)
        {
            return 1U;
        }
        if(0U == config_result)
        {
#if COMM_APP_ENABLE_POLL
            svc_poll_accept(g_rx, length);
#else
            g_hik.ignored_frames++;
#endif
        }
    }
    else if(SVC_SERIAL_EMPTY != result)
    {
        return 0U;
    }
    if(0U == flush_reply())
    {
        return 0U;
    }
    if(0U != g_reply_length)
    {
        return 1U;
    }
    result = svc_serial_read(HIK_HOST_PORT, g_rx, sizeof(g_rx), &length);
    if(SVC_SERIAL_EMPTY == result)
    {
        return 1U;
    }
    if(SVC_SERIAL_OK != result)
    {
        return 0U;
    }
    local = app_local_accept(HIK_HOST_PORT, g_rx, length);
    if(APP_LOCAL_ERROR == local || APP_LOCAL_RETRY == local)
    {
        return 0U;
    }
    if(APP_LOCAL_CONSUMED == local)
    {
        return 1U;
    }
    config_result = configuration(HIK_HOST_PORT, g_rx, length);
    if(3U == config_result)
    {
        return 0U;
    }
    if(0U != config_result)
    {
        return 1U;
    }
    return host_request(length);
}
uint8_t app_hik_task(void)
{
    uint8_t result = hik_task();
#if COMM_APP_ENABLE_POLL
    if(0U != svc_gm800_busy() ||
       (0U != app_local_busy() && 0U != app_local_maintenance_pending()))
    {
        svc_poll_pause();
    }
    else if(0U != result)
    {
        result = svc_poll_task();
    }
#endif
    return result;
}
uint8_t app_hik_get_status(app_hik_status_t *p_status)
{
    if(NULL == p_status)
    {
        return 0U;
    }
    *p_status = g_hik;
    return 1U;
}

/* end of file --------------------------------------------------------------*/
