/**
 ******************************************************************************
 *@file               :   app_dto.c
 *@brief              :   DTO broadcast and queued upstream replies.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "app_dto.h"
#include "app_local.h"
#include "proto_gm800.h"
#include "svc_gm800.h"
#include "svc_serial.h"
/* define   -----------------------------------------------------------------*/
#define DTO_DOWNLINK_MASK ((1U << COMM_PORT_RS485_1) | \
                           (1U << COMM_PORT_RS485_2))
#define DTO_UPLINK_MASK (1U << COMM_PORT_4G)

/* typedef ------------------------------------------------------------------*/
typedef struct DTO_PENDING_T
{
    uint16_t length;
    uint8_t  data[COMM_SERIAL_FRAME_SIZE];
} dto_pending_t;

/* variables ----------------------------------------------------------------*/
static dto_pending_t   g_pending[COMM_PORT_NUM];
static app_dto_stats_t g_stats;
static comm_port_t     g_next_reply = COMM_PORT_RS485_1;

/* private  functions  ------------------------------------------------------*/
static void clear_pending_config(comm_port_t source)
{
    uint32_t index;
    for(index = 0U; index < COMM_PORT_NUM; index++)
    {
        if(index != (uint32_t)source && 0U != g_pending[index].length)
        {
            g_stats.config_pending_dropped++;
        }
        g_pending[index].length = 0U;
    }
}
static uint8_t route_frame(comm_port_t source, uint32_t destination_mask)
{
    dto_pending_t      *p_pending = &g_pending[source];
    svc_serial_result_t result;
    if(0U == p_pending->length)
    {
        result = svc_serial_read(source, p_pending->data,
                                 sizeof(p_pending->data), &p_pending->length);
        if(SVC_SERIAL_EMPTY == result)
        {
            return 1U;
        }
        if(SVC_SERIAL_OK != result)
        {
            g_stats.errors++;
            return 0U;
        }
    }
    /* DTO ignores DD-prefixed frames received on either RS485 port.
     * Drop before routing so UART1 backpressure cannot retain these frames. */
    if(COMM_PORT_4G != source && 0U != p_pending->length &&
       0xDDU == p_pending->data[0U])
    {
        g_stats.dd_uplink_dropped[source - COMM_PORT_RS485_1]++;
        p_pending->length = 0U;
        return 1U;
    }
    {
        app_local_result_t local = app_local_accept(source, p_pending->data,
                                                    p_pending->length);
        if(APP_LOCAL_ERROR == local)
        {
            return 0U;
        }
        if(APP_LOCAL_RETRY == local)
        {
            return 1U;
        }
        if(APP_LOCAL_CONSUMED == local)
        {
            uint32_t index;
            p_pending->length = 0U;
            if(0U != app_local_maintenance_pending())
            {
                for(index = 0U; index < COMM_PORT_NUM; index++)
                {
                    if(0U != g_pending[index].length)
                    {
                        g_stats.local_pending_dropped++;
                        g_pending[index].length = 0U;
                    }
                }
            }
            return 1U;
        }
    }
    if(COMM_PORT_4G != source)
    {
        char identity[PROTO_GM800_ID_SIZE + 1U];
        proto_gm800_request_t
            request = proto_gm800_parse_request(p_pending->data,
                                                p_pending->length, identity,
                                                sizeof(identity));
        if(PROTO_GM800_INVALID_REQUEST == request)
        {
            g_stats.config_invalid++;
            p_pending->length = 0U;
            return 1U;
        }
        if(PROTO_GM800_VALID_REQUEST == request)
        {
            if(0U == svc_gm800_start(source, identity, sizeof(identity)))
            {
                g_stats.errors++;
                return 0U;
            }
            g_stats.config_requests++;
            clear_pending_config(source);
            return 1U;
        }
    }
    result = svc_serial_send_mask(destination_mask, p_pending->data,
                                  p_pending->length);
    if(SVC_SERIAL_FULL == result)
    {
        /* Retain this frame; do not consume another from the same source. */
        g_stats.blocked_attempts[source]++;
        return 1U;
    }
    if(SVC_SERIAL_OK != result)
    {
        g_stats.errors++;
        return 0U;
    }
    p_pending->length = 0U;
    if(COMM_PORT_4G == source)
    {
        g_stats.downlink_queued++;
    }
    else
    {
        g_stats.uplink_queued[source - COMM_PORT_RS485_1]++;
        /* Fairness when UART1 has room for only one of the two replies. */
        g_next_reply = COMM_PORT_RS485_1 == source ? COMM_PORT_RS485_2
                                                   : COMM_PORT_RS485_1;
    }
    return 1U;
}

/* exported functions -------------------------------------------------------*/
void app_dto_discard_pending(void)
{
    uint32_t index;
    for(index = 0U; index < COMM_PORT_NUM; index++)
    {
        g_pending[index].length = 0U;
    }
    g_next_reply = COMM_PORT_RS485_1;
}
uint8_t app_dto_task(void)
{
    comm_port_t first  = g_next_reply;
    comm_port_t second = COMM_PORT_RS485_1 == first ? COMM_PORT_RS485_2
                                                    : COMM_PORT_RS485_1;
    if(0U != svc_gm800_busy() || 0U != app_local_busy())
    {
        return 1U;
    }
    if(0U == route_frame(first, DTO_UPLINK_MASK))
    {
        return 0U;
    }
    if(0U != svc_gm800_busy() || 0U != app_local_busy())
    {
        return 1U;
    }
    if(0U == route_frame(second, DTO_UPLINK_MASK))
    {
        return 0U;
    }
    if(0U != svc_gm800_busy() || 0U != app_local_busy())
    {
        return 1U;
    }
    return route_frame(COMM_PORT_4G, DTO_DOWNLINK_MASK);
}
uint8_t app_dto_get_stats(app_dto_stats_t *p_stats)
{
    if(NULL == p_stats)
    {
        return 0U;
    }
    *p_stats = g_stats;
    return 1U;
}

/* end of file --------------------------------------------------------------*/
