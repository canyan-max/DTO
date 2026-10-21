/**
 ******************************************************************************
 *@file               :   app_xdz.c
 *@brief              :   Exclusive XDZ RX dispatch, shared maintenance and queued replies.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include <string.h>
#include "app_xdz.h"
#include "app_local.h"
#include "bsp_system.h"
#include "svc_gm800.h"
#include "svc_serial.h"
#include "svc_xdz_poll.h"
#include "proto_gm800.h"
#include "proto_xdz.h"
#include "xdz_config.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/
app_xdz_status_t g_xdz;
static uint8_t   g_rx[COMM_SERIAL_FRAME_SIZE];
static uint8_t   g_reply[XDZ_MAX_REPLY];
static uint16_t  g_reply_length;
static uint32_t  g_reply_ms;

/* private  functions  ------------------------------------------------------*/

static uint8_t flush_reply(void)
{
    svc_serial_result_t result;
    if(0U == g_reply_length)
    {
        return 1U;
    }
    if((uint32_t)(bsp_system_time_ms() - g_reply_ms) >= XDZ_REPLY_QUEUE_MS)
    {
        g_xdz.reply_timeouts++;
        g_reply_length = 0U;
        return 1U;
    }
    result = svc_serial_send(XDZ_HOST_PORT, g_reply, g_reply_length);
    if(SVC_SERIAL_FULL == result)
    {
        g_xdz.blocked++;
        return 1U;
    }
    if(SVC_SERIAL_OK != result)
    {
        return 0U;
    }
    g_reply_length = 0U;
    g_xdz.replies_queued++;
    return 1U;
}
static uint8_t dispatch(comm_port_t source, uint16_t length)
{
    app_local_result_t    local = app_local_accept(source, g_rx, length);
    char                  identity[PROTO_GM800_ID_SIZE + 1U];
    proto_gm800_request_t config;
    if(APP_LOCAL_ERROR == local || APP_LOCAL_RETRY == local)
    {
        return 0U;
    }
    if(APP_LOCAL_CONSUMED == local)
    {
        return 1U;
    }
    if(COMM_PORT_4G == source)
    {
        g_xdz.dropped++;
        return 1U;
    }
    config = proto_gm800_parse_request(g_rx, length, identity,
                                       sizeof(identity));
    if(PROTO_GM800_INVALID_REQUEST == config)
    {
        g_xdz.dropped++;
        return 1U;
    }
    if(PROTO_GM800_VALID_REQUEST == config)
    {
        if(0U == svc_gm800_start(source, identity, sizeof(identity)))
        {
            return 0U;
        }
        g_xdz.configuration_requests++;
        app_xdz_discard_pending();
        return 1U;
    }
    if(XDZ_DEVICE_PORT == source)
    {
        svc_xdz_poll_accept(g_rx, length);
        return 1U;
    }
    {
        proto_xdz_data_t        snapshot;
        uint8_t                 valid, level = 0U;
        proto_xdz_host_result_t result;
        if(0U == svc_xdz_poll_get(&snapshot, &valid))
        {
            return 0U;
        }
        result = proto_xdz_host_process(g_rx, length, &snapshot, &level,
                                        g_reply, sizeof(g_reply),
                                        &g_reply_length);
        if(PROTO_XDZ_HOST_DROP == result)
        {
            g_xdz.dropped++;
            return 1U;
        }
        g_xdz.requests++;
        if(0U == valid)
        {
            g_xdz.no_data++;
        }
        if(PROTO_XDZ_HOST_RELAY == result)
        {
            local = app_local_reply_relay_pair(source, g_reply, g_reply_length,
                                               level);
            g_reply_length = 0U;
            if(APP_LOCAL_CONSUMED != local)
            {
                return 0U;
            }
            g_xdz.relay_commands++;
            return 1U;
        }
        g_reply_ms = bsp_system_time_ms();
        return flush_reply();
    }
}
static uint8_t xdz_task(void)
{
    uint32_t          index;
    uint16_t          length;
    const comm_port_t ports[] = {COMM_PORT_4G, XDZ_DEVICE_PORT, XDZ_HOST_PORT};
    if(0U != svc_gm800_busy())
    {
        app_xdz_discard_pending();
        return 1U;
    }
    if(0U != app_local_busy())
    {
        if(0U != app_local_maintenance_pending())
        {
            g_reply_length = 0U;
        }
        return 1U;
    }
    if(0U == flush_reply())
    {
        return 0U;
    }
    for(index = 0U; index < sizeof(ports) / sizeof(ports[0U]); index++)
    {
        svc_serial_result_t result;
        if(XDZ_HOST_PORT == ports[index] && 0U != g_reply_length)
        {
            continue;
        }
        result = svc_serial_read(ports[index], g_rx, sizeof(g_rx), &length);
        if(SVC_SERIAL_EMPTY == result)
        {
            continue;
        }
        if(SVC_SERIAL_OK != result || 0U == dispatch(ports[index], length))
        {
            return 0U;
        }
        if(0U != svc_gm800_busy() || 0U != app_local_busy())
        {
            return 1U;
        }
    }
    return 1U;
}

/* exported functions -------------------------------------------------------*/

void app_xdz_init(void)
{
    memset(&g_xdz, 0, sizeof(g_xdz));
    g_reply_length = 0U;
    svc_xdz_poll_init();
}
void app_xdz_discard_pending(void)
{
    g_reply_length = 0U;
    svc_xdz_poll_pause();
}
uint8_t app_xdz_task(void)
{
    uint8_t result = xdz_task();
    if(0U != svc_gm800_busy() ||
       (0U != app_local_busy() && 0U != app_local_maintenance_pending()))
    {
        svc_xdz_poll_pause();
    }
    else if(0U != result)
    {
        result = svc_xdz_poll_task();
    }
    return result;
}

/* end of file --------------------------------------------------------------*/
