/**
 ******************************************************************************
 *@file               :   app_local.c
 *@brief              :   Local parameter replies and deferred save maintenance.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include <string.h>
#include "app_local.h"
#include "bsp_system.h"
#include "bsp_uart.h"
#include "proto_local.h"
#include "svc_relay.h"
#include "dto_config.h"
#include "hik_config.h"
#include "svc_gm800.h"
#include "svc_params.h"
#include "svc_boot.h"
#include "svc_serial.h"
#include "svc_telemetry.h"
#if COMM_APP_ENABLE_MODE
#include "app_mode.h"
#endif
/* define   -----------------------------------------------------------------*/
#define LOCAL_TRANSACTION_TIMEOUT_MS (20000U)

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/
static app_local_status_t   g_local_status;
static uint8_t              g_reply[COMM_SERIAL_FRAME_SIZE];
static uint16_t             g_reply_length;
static proto_local_action_t g_action;
static uint32_t             g_started_ms;
static uint32_t             g_tx_errors;
static uint8_t              g_relay_save;
static uint8_t              g_requested_mode;
static comm_port_t          g_reply_port;
static uint32_t             g_boot_tx_errors[COMM_PORT_NUM];

/* private  functions  ------------------------------------------------------*/
static uint8_t fail_transaction(void)
{
    g_local_status.state = APP_LOCAL_FAILED;
    g_local_status.failures++;
    g_action     = PROTO_LOCAL_ACTION_NONE;
    g_relay_save = 0U;
    return 1U;
}
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

static app_local_result_t
accept_local(comm_port_t source, const uint8_t *p_data, uint16_t length)
{
    comm_params_t            current;
    comm_params_t            next;
    proto_local_action_t     action;
    proto_local_result_t     result;
    svc_serial_stats_t       stats;
    svc_relay_status_t       relay;
    proto_local_command_t    command;
    proto_local_snapshot_t   snapshot;
    svc_telemetry_snapshot_t telemetry;
    if(NULL == p_data || 0U == length || PROTO_LOCAL_ADDRESS != p_data[0U])
    {
        return APP_LOCAL_NOT_LOCAL;
    }
    if(0U != app_local_busy() || 0U != svc_gm800_busy())
    {
        return APP_LOCAL_RETRY;
    }
    if(0U == svc_params_get(&current) || 0U == svc_relay_get_status(&relay) ||
       0U == svc_telemetry_get(&telemetry))
    {
        return APP_LOCAL_ERROR;
    }
    snapshot.params           = current;
    snapshot.relay_output[0U] = relay.output[0U];
    snapshot.relay_output[1U] = relay.output[1U];
#if COMM_APP_ENABLE_MODE
    memcpy(snapshot.version,
           COMM_APP_MODE_HIK == current.app_mode ? HIK_FIRMWARE_VERSION
                                                 : DTO_FIRMWARE_VERSION,
           sizeof(snapshot.version));
#elif COMM_APP_ENABLE_HIK
    memcpy(snapshot.version, HIK_FIRMWARE_VERSION, sizeof(snapshot.version));
#else
    memcpy(snapshot.version, DTO_FIRMWARE_VERSION, sizeof(snapshot.version));
#endif
    snapshot.battery       = telemetry.battery;
    snapshot.solar         = telemetry.solar;
    snapshot.battery_valid = telemetry.battery_valid;
    snapshot.solar_valid   = telemetry.solar_valid;
    result = proto_local_process(p_data, length, &snapshot, &command, g_reply,
                                 sizeof(g_reply), &g_reply_length);
    if(PROTO_LOCAL_REPLY != result)
    {
        g_local_status.dropped++;
        return APP_LOCAL_CONSUMED;
    }
    next   = command.params;
    action = command.action;
    /* Mode changes apply only after a successful ACK and TX drain. */
    if(PROTO_LOCAL_ACTION_MODE == action && 0U == (g_reply[1U] & 0x80U))
    {
        g_requested_mode = next.app_mode;
        next.app_mode    = current.app_mode;
        if(g_requested_mode == current.app_mode)
        {
            action = PROTO_LOCAL_ACTION_NONE;
        }
    }
    /* Error replies must not apply a partially decoded proposal. */
    if(0U == (g_reply[1U] & 0x80U) && 0U == svc_params_set(&next))
    {
        return APP_LOCAL_ERROR;
    }
    g_relay_save = 0U;
    if(0U == (g_reply[1U] & 0x80U) && 0U != command.write_relay)
    {
        if(0U == svc_relay_set(command.relay, command.level))
        {
            return APP_LOCAL_ERROR;
        }
        g_relay_save = 1U;
    }
    g_reply_port = source;
    if(0U == svc_serial_get_stats(g_reply_port, &stats))
    {
        return APP_LOCAL_ERROR;
    }
    g_action = 0U != (g_reply[1U] & 0x80U) ? PROTO_LOCAL_ACTION_NONE : action;
    g_tx_errors  = stats.tx_errors;
    g_started_ms = bsp_system_time_ms();
    g_local_status.requests++;
    g_local_status.state = APP_LOCAL_REPLY_PENDING;
    return APP_LOCAL_CONSUMED;
}
/* exported functions -------------------------------------------------------*/
app_local_result_t
app_local_accept(comm_port_t source, const uint8_t *p_data, uint16_t length)
{
    if((uint32_t)source >= COMM_PORT_NUM)
    {
        return APP_LOCAL_ERROR;
    }
    if(0U != proto_local_is_boot_request(p_data, length))
    {
        uint32_t           index;
        svc_serial_stats_t stats;
        if(0U != app_local_busy() || 0U != svc_gm800_busy())
        {
            return APP_LOCAL_RETRY;
        }
        g_local_status.requests++;
        if(0U == svc_boot_ready())
        {
            fail_transaction();
            return APP_LOCAL_CONSUMED;
        }
        for(index = 0U; index < COMM_PORT_NUM; index++)
        {
            if(0U == svc_serial_get_stats((comm_port_t)index, &stats))
            {
                return APP_LOCAL_ERROR;
            }
            g_boot_tx_errors[index] = stats.tx_errors;
        }
        g_reply_port   = source;
        g_tx_errors    = g_boot_tx_errors[source];
        g_started_ms   = bsp_system_time_ms();
        g_reply_length = 0U;
        g_relay_save   = 0U;
        g_action       = PROTO_LOCAL_ACTION_BOOT;
        /* The legacy command has no ACK; drain previously owned TX buffers. */
        g_local_status.state = APP_LOCAL_WAIT_TX;
        return APP_LOCAL_CONSUMED;
    }
    return accept_local(source, p_data, length);
}
uint8_t app_local_busy(void)
{
    return APP_LOCAL_REPLY_PENDING == g_local_status.state ||
           APP_LOCAL_WAIT_TX == g_local_status.state ||
           APP_LOCAL_RESET_REQUESTED == g_local_status.state;
}
uint8_t app_local_maintenance_pending(void)
{
    return 0U != app_local_busy() &&
           (PROTO_LOCAL_ACTION_NONE != g_action || 0U != g_relay_save);
}
uint8_t app_local_task(void)
{
    svc_serial_result_t result;
    svc_serial_stats_t  stats;
    uint16_t            pending;
    uint32_t            index;
    uint8_t             saved;
    if(0U == app_local_busy() ||
       APP_LOCAL_RESET_REQUESTED == g_local_status.state)
    {
        return 1U;
    }
    if((uint32_t)(bsp_system_time_ms() - g_started_ms) >=
       LOCAL_TRANSACTION_TIMEOUT_MS)
    {
        return fail_transaction();
    }
    if(0U == svc_serial_get_stats(g_reply_port, &stats))
    {
        return 0U;
    }
    if(PROTO_LOCAL_ACTION_BOOT == g_action)
    {
        for(index = 0U; index < COMM_PORT_NUM; index++)
        {
            if(0U == svc_serial_get_stats((comm_port_t)index, &stats))
            {
                return 0U;
            }
            if(stats.tx_errors != g_boot_tx_errors[index])
            {
                return fail_transaction();
            }
        }
        if(0U == svc_serial_get_stats(g_reply_port, &stats))
        {
            return 0U;
        }
    }
    if(stats.tx_errors != g_tx_errors)
    {
        return fail_transaction();
    }
    if(APP_LOCAL_REPLY_PENDING == g_local_status.state)
    {
        result = svc_serial_send(g_reply_port, g_reply, g_reply_length);
        if(SVC_SERIAL_FULL == result)
        {
            return 1U;
        }
        if(SVC_SERIAL_OK != result)
        {
            return fail_transaction();
        }
        g_local_status.state = APP_LOCAL_WAIT_TX;
        return 1U;
    }
    if(0U == svc_serial_tx_pending(g_reply_port, &pending))
    {
        return 0U;
    }
    if(0U != pending)
    {
        return 1U;
    }
    if(PROTO_LOCAL_ACTION_NONE == g_action && 0U == g_relay_save)
    {
        g_local_status.state = APP_LOCAL_DONE;
        return 1U;
    }
    /* The reply and all pre-existing traffic must leave before Flash access. */
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
    if(PROTO_LOCAL_ACTION_MODE == g_action)
    {
#if COMM_APP_ENABLE_MODE
        if(0U == app_mode_request((comm_app_mode_t)g_requested_mode))
        {
            return fail_transaction();
        }
#else
        /* Bench-only single-mode builds never expose runtime switching. */
        return fail_transaction();
#endif
        g_action             = PROTO_LOCAL_ACTION_NONE;
        g_local_status.state = APP_LOCAL_DONE;
        return 1U;
    }
    if(0U == bsp_uart_suspend_rx(1U) || 0U == clear_rx())
    {
        return 0U;
    }
    if(PROTO_LOCAL_ACTION_BOOT == g_action)
    {
        saved = svc_boot_request();
    }
    else if(0U != g_relay_save)
    {
        saved = svc_relay_save();
    }
    else
    {
        saved = PROTO_LOCAL_ACTION_SAVE == g_action
                    ? svc_params_save()
                    : svc_params_save_defaults();
    }
    if(0U == saved)
    {
        if(0U == bsp_uart_suspend_rx(0U) || 0U == clear_rx())
        {
            return 0U;
        }
        return fail_transaction();
    }
    if(PROTO_LOCAL_ACTION_BOOT != g_action)
    {
        g_local_status.saves++;
    }
    if(0U != g_relay_save)
    {
        g_relay_save = 0U;
        if(0U == bsp_uart_suspend_rx(0U) || 0U == clear_rx())
        {
            return 0U;
        }
        g_local_status.state = APP_LOCAL_DONE;
        return 1U;
    }
    g_local_status.state = APP_LOCAL_RESET_REQUESTED;
    /* Reset only after verified save, with no active TX/RX buffer users. */
    bsp_system_reset();
    return 1U;
}
uint8_t app_local_get_status(app_local_status_t *p_status)
{
    if(NULL == p_status)
    {
        return 0U;
    }
    *p_status = g_local_status;
    return 1U;
}

/* end of file --------------------------------------------------------------*/
