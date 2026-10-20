/**
 ******************************************************************************
 *@file               :   svc_gm800.c
 *@brief              :   GM800 setup with bounded per-command acknowledgement.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include "svc_gm800.h"
#include "bsp_system.h"
#include "gm800_config.h"
#include "proto_gm800.h"
#include "svc_serial.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/
static svc_gm800_status_t     g_status;
static char                   g_identity[PROTO_GM800_ID_SIZE + 1U];
static uint8_t                g_buffer[COMM_SERIAL_FRAME_SIZE];
static uint32_t               g_started_ms;
static uint32_t               g_last_command_ms;
static uint32_t               g_ack_started_ms;
static uint32_t               g_tx_errors;
static uint32_t               g_rx_errors;
static uint32_t               g_rx_oversize;
static uint32_t               g_rx_full;
static uint8_t                g_waiting_tx;
static uint8_t                g_awaiting_ack;
static uint8_t                g_response_result;
static comm_port_t            g_log_port;
static uint32_t               g_tail_started_ms;
static svc_gm800_state_t      g_finish_state;
static uint8_t                g_notice_sent;
static proto_gm800_response_t g_response;

/* private  functions  ------------------------------------------------------*/
static uint8_t clear_rx(void)
{
    uint32_t index;
    for(index = 0U; index < COMM_PORT_NUM; index++)
    {
        if(0U == svc_serial_discard_rx((comm_port_t)index))
        {
            return 0U;
        }
    }
    return 1U;
}
static void begin_finish(svc_gm800_state_t state)
{
    g_finish_state       = state;
    g_status.failed_step = SVC_GM800_CONFIRMED == state
                               ? 0U
                               : (uint8_t)(g_status.commands_confirmed +
                                           (0U != g_awaiting_ack ? 1U : 0U));
    g_awaiting_ack       = 0U;
    g_waiting_tx         = 0U;
    g_tail_started_ms    = bsp_system_time_ms();
    g_notice_sent        = 0U;
    g_status.state       = SVC_GM800_FINISHING;
}
static uint8_t finish_task(void)
{
    uint32_t    index;
    uint16_t    pending;
    char        notice[80];
    int         length;
    const char *p_reason;
    if((uint32_t)(bsp_system_time_ms() - g_tail_started_ms) < GM800_LOG_TAIL_MS)
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
    if(0U == g_notice_sent)
    {
        if(SVC_GM800_CONFIRMED == g_finish_state)
        {
            length = snprintf(notice, sizeof(notice),
                              "\r\n[GM800] CONFIG CONFIRMED\r\n");
        }
        else
        {
            p_reason = SVC_GM800_RESPONSE_ERROR == g_finish_state
                           ? "ERROR"
                           : (SVC_GM800_RESPONSE_TIMEOUT == g_finish_state
                                  ? "ACK TIMEOUT"
                                  : (SVC_GM800_TIMEOUT == g_finish_state
                                         ? "SESSION TIMEOUT"
                                         : "TRANSPORT ERROR"));
            length =
                snprintf(notice, sizeof(notice),
                         "\r\n[GM800] FAIL step=%u %s; SEND CONFIG AGAIN\r\n",
                         (unsigned)g_status.failed_step, p_reason);
        }
        if(length <= 0 || (size_t)length >= sizeof(notice) ||
           SVC_SERIAL_OK != svc_serial_send(g_log_port, (const uint8_t *)notice,
                                            (uint16_t)length))
        {
            return 0U;
        }
        g_notice_sent = 1U;
        return 1U;
    }
    if(0U == clear_rx())
    {
        return 0U;
    }
    g_status.state = g_finish_state;
    if(SVC_GM800_CONFIRMED == g_finish_state)
    {
        g_status.sequences_sent++;
    }
    else
    {
        g_status.failures++;
    }
    return 1U;
}
static uint8_t consume_rx(void)
{
    uint32_t            index;
    uint32_t            count;
    uint16_t            length;
    svc_serial_result_t result;
    for(index = 0U; index < COMM_PORT_NUM; index++)
    {
        for(count = 0U; count < COMM_SERIAL_QUEUE_DEPTH; count++)
        {
            result = svc_serial_read((comm_port_t)index, g_buffer,
                                     sizeof(g_buffer), &length);
            if(SVC_SERIAL_EMPTY == result)
            {
                break;
            }
            if(SVC_SERIAL_OK != result)
            {
                return 0U;
            }
            if(COMM_PORT_4G == index)
            {
                g_status.module_rx_frames++;
                if(0U != g_awaiting_ack)
                {
                    g_response_result |= proto_gm800_response_feed(
                        &g_response, g_status.commands_confirmed, g_buffer,
                        length);
                }
                if(SVC_GM800_FINISHING == g_status.state &&
                   (uint32_t)(bsp_system_time_ms() - g_tail_started_ms) >=
                       GM800_LOG_TAIL_MS)
                {
                    continue;
                }
                /* Diagnostics are best effort; protocol checking is
                 * independent. */
                if(SVC_SERIAL_OK ==
                   svc_serial_send(g_log_port, g_buffer, length))
                {
                    g_status.log_frames++;
                }
                else
                {
                    g_status.log_dropped++;
                }
            }
            else
            {
                g_status.suppressed_frames++;
            }
        }
    }
    return 1U;
}

/* exported functions -------------------------------------------------------*/
uint8_t
svc_gm800_start(comm_port_t source, const char *p_identity, uint16_t capacity)
{
    uint16_t index;
    if((COMM_PORT_RS485_1 != source && COMM_PORT_RS485_2 != source) ||
       NULL == p_identity || capacity < PROTO_GM800_ID_SIZE + 1U ||
       0U != svc_gm800_busy())
    {
        return 0U;
    }
    for(index = 0U; index < PROTO_GM800_ID_SIZE; index++)
    {
        if(p_identity[index] < '0' || p_identity[index] > '9')
        {
            return 0U;
        }
    }
    memcpy(g_identity, p_identity, PROTO_GM800_ID_SIZE);
    g_identity[PROTO_GM800_ID_SIZE] = '\0';
    g_status.state                  = SVC_GM800_DRAINING;
    g_status.commands_sent          = 0U;
    g_status.commands_confirmed     = 0U;
    g_status.failed_step            = 0U;
    g_status.sessions_started++;
    g_started_ms      = bsp_system_time_ms();
    g_waiting_tx      = 0U;
    g_awaiting_ack    = 0U;
    g_response_result = 0U;
    proto_gm800_response_reset(&g_response);
    g_log_port = source;
    return 1U;
}
uint8_t svc_gm800_busy(void)
{
    return SVC_GM800_DRAINING == g_status.state ||
           SVC_GM800_SENDING == g_status.state ||
           SVC_GM800_FINISHING == g_status.state;
}
uint8_t svc_gm800_task(void)
{
    uint32_t            now_ms;
    uint32_t            index;
    uint32_t            timeout_ms;
    uint16_t            pending;
    uint16_t            length;
    svc_serial_stats_t  stats;
    svc_serial_result_t result;
    if(0U == svc_gm800_busy())
    {
        return 1U;
    }
    now_ms = bsp_system_time_ms();
    if(0U == consume_rx())
    {
        return 0U;
    }
    if(SVC_GM800_FINISHING == g_status.state)
    {
        return finish_task();
    }
    if((uint32_t)(now_ms - g_started_ms) >= GM800_SESSION_TIMEOUT_MS)
    {
        begin_finish(SVC_GM800_TIMEOUT);
        return 1U;
    }
    if(SVC_GM800_DRAINING == g_status.state)
    {
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
        if(0U == clear_rx() || 0U == svc_serial_get_stats(COMM_PORT_4G, &stats))
        {
            return 0U;
        }
        g_tx_errors       = stats.tx_errors;
        g_rx_errors       = stats.rx_errors;
        g_rx_oversize     = stats.rx_oversize;
        g_rx_full         = stats.rx_queue_full;
        g_last_command_ms = now_ms;
        g_status.state    = SVC_GM800_SENDING;
        return 1U;
    }
    if(0U == svc_serial_get_stats(COMM_PORT_4G, &stats) ||
       0U == svc_serial_tx_pending(COMM_PORT_4G, &pending))
    {
        return 0U;
    }
    if(stats.tx_errors != g_tx_errors || stats.rx_errors != g_rx_errors ||
       stats.rx_oversize != g_rx_oversize || stats.rx_queue_full != g_rx_full)
    {
        begin_finish(SVC_GM800_TRANSPORT_ERROR);
        return 1U;
    }
    if(0U != g_waiting_tx)
    {
        if(0U != pending)
        {
            return 1U;
        }
        g_waiting_tx = 0U;
        g_status.commands_sent++;
        g_ack_started_ms = now_ms;
    }
    if(0U != g_awaiting_ack)
    {
        if(0U != (g_response_result & PROTO_GM800_RESPONSE_ERROR))
        {
            begin_finish(SVC_GM800_RESPONSE_ERROR);
            return 1U;
        }
        if(0U != (g_response_result & PROTO_GM800_RESPONSE_OK))
        {
            g_status.commands_confirmed++;
            g_awaiting_ack = 0U;
            if(PROTO_GM800_COMMAND_COUNT == g_status.commands_confirmed)
            {
                begin_finish(SVC_GM800_CONFIRMED);
                return 1U;
            }
        }
        else
        {
            timeout_ms = PROTO_GM800_STEP_SAVE_FIRST ==
                                     g_status.commands_confirmed ||
                                 PROTO_GM800_STEP_SAVE_SECOND ==
                                     g_status.commands_confirmed
                             ? GM800_SAVE_TIMEOUT_MS
                             : GM800_RESPONSE_TIMEOUT_MS;
            if((uint32_t)(now_ms - g_ack_started_ms) >= timeout_ms)
            {
                begin_finish(SVC_GM800_RESPONSE_TIMEOUT);
            }
            return 1U;
        }
    }
    if((uint32_t)(now_ms - g_last_command_ms) < GM800_STEP_INTERVAL_MS)
    {
        return 1U;
    }
    if(0U == proto_gm800_command(g_status.commands_confirmed, g_identity,
                                 g_buffer, sizeof(g_buffer), &length))
    {
        return 0U;
    }
    result = svc_serial_send(COMM_PORT_4G, g_buffer, length);
    if(SVC_SERIAL_FULL == result)
    {
        return 1U;
    }
    if(SVC_SERIAL_OK != result)
    {
        begin_finish(SVC_GM800_TRANSPORT_ERROR);
        return 1U;
    }
    proto_gm800_response_reset(&g_response);
    g_response_result = 0U;
    g_last_command_ms = now_ms;
    g_waiting_tx      = 1U;
    g_awaiting_ack    = 1U;
    return 1U;
}
uint8_t svc_gm800_get_status(svc_gm800_status_t *p_status)
{
    if(NULL == p_status)
    {
        return 0U;
    }
    *p_status = g_status;
    return 1U;
}

/* end of file --------------------------------------------------------------*/
