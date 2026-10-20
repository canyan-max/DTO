/**
 ******************************************************************************
 *@file               :   svc_relay.c
 *@brief              :   Persistent command state separate from transient UV.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include <string.h>
#include "svc_relay.h"
#include "bsp_io.h"
#include "bsp_nv.h"
#include "bsp_system.h"
#include "svc_params.h"
#include "svc_sampling.h"
#include "svc_serial.h"
#include "svc_gm800.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/
static svc_relay_status_t g_relay;
static comm_params_t      g_protection_params;
static uint32_t           g_dwell_ms;
static uint8_t            g_dwell_active;
static uint32_t           g_last_task_ms;

/* private  functions  ------------------------------------------------------*/
static uint8_t apply_outputs(void)
{
    uint32_t      index;
    comm_params_t params;
    if(0U == svc_params_get(&params))
    {
        return 0U;
    }
    for(index = 0U; index < DTO_RELAY_COUNT; index++)
    {
        uint8_t logical = 0U != g_relay.uv_active ? 0U
                                                  : g_relay.requested[index];
        uint8_t invert  = (uint8_t)((params.relay_contact_mask >> index) & 1U);
        uint8_t gpio    = logical ^ invert;
        /* NC=0 keeps the original GPIO behavior. NO=1 inverts both manual
         * requests and UV override through this same physical mapping. */
        if(g_relay.gpio[index] != gpio)
        {
            if(0U == bsp_io_set_relay((uint8_t)index, gpio))
            {
                g_relay.failures++;
                return 0U;
            }
            g_relay.gpio[index] = gpio;
        }
        g_relay.output[index] = logical;
    }
    return 1U;
}

/* exported functions -------------------------------------------------------*/
uint8_t svc_relay_init(void)
{
    uint64_t saved;
    uint32_t index;
    memset(&g_relay, 0, sizeof(g_relay));
    memset(&g_protection_params, 0, sizeof(g_protection_params));
    g_dwell_active = 0U;
    g_last_task_ms = bsp_system_time_ms();
    /* CubeMX starts low. Default NC=0 preserves those levels; a saved NO
     * contact setting is applied with the remembered logical request. */
    if(0U != bsp_nv_read_relays(&saved) && saved <= 15U)
    {
        for(index = 0U; index < DTO_RELAY_COUNT; index++)
        {
            g_relay.requested[index] = (uint8_t)((saved >> index) & 1U);
        }
        g_relay.from_flash = 1U;
    }
    return apply_outputs();
}
uint8_t svc_relay_set(uint8_t relay, uint8_t level)
{
    if(relay >= DTO_RELAY_COUNT || level > 1U)
    {
        return 0U;
    }
    if(g_relay.requested[relay] != level)
    {
        g_relay.requested[relay] = level;
        g_relay.dirty            = 1U;
    }
    return apply_outputs();
}
uint8_t svc_relay_task(void)
{
    comm_params_t         params;
    svc_sampling_status_t sampling;
    uint32_t              now_ms = bsp_system_time_ms();
    uint8_t               condition;
    uint32_t              duration_ms;
    if((uint32_t)(now_ms - g_last_task_ms) >= 1000U)
    {
        /* A stalled scheduler cannot prove continuously valid voltage. */
        g_dwell_active = 0U;
    }
    g_last_task_ms = now_ms;
    if(0U == svc_params_get(&params) ||
       0U == svc_sampling_get_status(&sampling))
    {
        return 0U;
    }
    if(params.uv_protect_mv != g_protection_params.uv_protect_mv ||
       params.uv_recover_mv != g_protection_params.uv_recover_mv ||
       params.uv_enabled != g_protection_params.uv_enabled)
    {
        g_dwell_active      = 0U;
        g_protection_params = params;
    }
    g_relay.sample_invalid = 0U == sampling.valid ? 1U : 0U;
    if(0U == params.uv_enabled)
    {
        g_dwell_active    = 0U;
        g_relay.uv_active = 0U;
        return apply_outputs();
    }
    if(0U == sampling.valid)
    {
        /* Invalid/stale sampling neither invents zero volts nor recovers UV. */
        g_dwell_active = 0U;
        return apply_outputs();
    }
    condition   = 0U == g_relay.uv_active
                      ? sampling.input_mv <= params.uv_protect_mv
                      : sampling.input_mv > params.uv_recover_mv;
    duration_ms = 0U == g_relay.uv_active ? DTO_UV_PROTECT_MS
                                          : DTO_UV_RECOVER_MS;
    if(0U == condition)
    {
        g_dwell_active = 0U;
    }
    else if(0U == g_dwell_active)
    {
        g_dwell_active = 1U;
        g_dwell_ms     = now_ms;
    }
    else if((uint32_t)(now_ms - g_dwell_ms) >= duration_ms)
    {
        g_relay.uv_active = 0U == g_relay.uv_active ? 1U : 0U;
        g_dwell_active    = 0U;
        if(0U != g_relay.uv_active)
        {
            g_relay.protection_entries++;
        }
        else
        {
            g_relay.protection_recoveries++;
        }
    }
    return apply_outputs();
}
uint8_t svc_relay_save(void)
{
    uint64_t word;
    uint64_t verify;
    uint16_t pending;
    uint32_t index;
    if(0U != svc_gm800_busy())
    {
        return 0U;
    }
    for(index = 0U; index < COMM_PORT_NUM; index++)
    {
        if(0U == svc_serial_tx_pending((comm_port_t)index, &pending) ||
           0U != pending)
        {
            return 0U;
        }
    }
    word = (uint64_t)g_relay.requested[0U] |
           ((uint64_t)g_relay.requested[1U] << 1U);
    if(0U != bsp_nv_read_relays(&verify) && verify <= 15U &&
       (verify & 3U) == word)
    {
        g_relay.dirty      = 0U;
        g_relay.from_flash = 1U;
        return 1U;
    }
    if(0U == bsp_nv_write_relays(word) || 0U == bsp_nv_read_relays(&verify) ||
       word != verify)
    {
        g_relay.failures++;
        return 0U;
    }
    g_relay.dirty      = 0U;
    g_relay.from_flash = 1U;
    g_relay.saves++;
    return 1U;
}
uint8_t svc_relay_get_status(svc_relay_status_t *p_status)
{
    if(NULL == p_status)
    {
        return 0U;
    }
    *p_status = g_relay;
    return 1U;
}

/* end of file --------------------------------------------------------------*/
