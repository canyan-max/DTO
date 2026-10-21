/**
 ******************************************************************************
 *@file               :   proto_local.c
 *@brief              :   Local relay, parameter, version and mode register codec.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include <string.h>
#include "proto_local.h"
#include "boot_config.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* private  functions  ------------------------------------------------------*/
static uint16_t get_u16(const uint8_t *p_data)
{
    return ((uint16_t)p_data[0U] << 8U) | p_data[1U];
}
static uint32_t get_u32(const uint8_t *p_data)
{
    return ((uint32_t)get_u16(p_data) << 16U) | get_u16(p_data + 2U);
}
static void append_crc(uint8_t *p_data, uint16_t length)
{
    uint16_t crc        = proto_local_crc16(p_data, length);
    p_data[length]      = (uint8_t)crc;
    p_data[length + 1U] = (uint8_t)(crc >> 8U);
}
static proto_local_result_t
exception(uint8_t function, uint8_t code, uint8_t *p_output, uint16_t *p_length)
{
    p_output[0U] = PROTO_LOCAL_ADDRESS;
    p_output[1U] = function | 0x80U;
    p_output[2U] = code;
    append_crc(p_output, 3U);
    *p_length = 5U;
    return PROTO_LOCAL_REPLY;
}
static uint16_t register_value(uint16_t                      address,
                               const proto_local_snapshot_t *p_snapshot)
{
    const comm_params_t *p_params = &p_snapshot->params;
    if(address >= 21U && address <= 25U)
    {
        uint16_t index = (uint16_t)((address - 21U) * 2U);
        return ((uint16_t)p_snapshot->version[index] << 8U) |
               p_snapshot->version[index + 1U];
    }
    if((address >= 36U && address <= 41U && 0U == p_snapshot->battery_valid) ||
       (address >= 42U && address <= 43U && 0U == p_snapshot->solar_valid))
    {
        return 0U;
    }
    switch(address)
    {
        case 17U:
            return p_snapshot->relay_output[0U];
        case 18U:
            return p_snapshot->relay_output[1U];
        case 19U:
            return 0U == p_snapshot->relay_output[0U] ? 1U : 0U;
        case 20U:
            return 0U == p_snapshot->relay_output[1U] ? 1U : 0U;
        case 26U:
            return p_params->uv_protect_mv;
        case 27U:
            return p_params->uv_recover_mv;
        case 28U:
            return p_params->uv_enabled;
        case 29U:
            return (uint16_t)(p_params->rs485_1_baud >> 16U);
        case 30U:
            return (uint16_t)p_params->rs485_1_baud;
        case 31U:
            return (uint16_t)(p_params->rs485_2_baud >> 16U);
        case 32U:
            return (uint16_t)p_params->rs485_2_baud;
        case 34U:
            return p_params->app_mode;
        case 35U:
            return p_params->relay_contact_mask;
        case 36U:
            return p_snapshot->battery.voltage_10mv;
        case 37U:
            return (uint16_t)p_snapshot->battery.current_10ma;
        case 38U:
            return p_snapshot->battery.soc_percent;
        case 39U:
            /* XDZ local reporting has an extra /10; retain shared cache
             * units and the external XDZ/HIK wire formats unchanged. */
            return COMM_APP_MODE_XDZ == p_params->app_mode
                       ? p_snapshot->battery.remaining_10mah / 10U
                       : p_snapshot->battery.remaining_10mah;
        case 40U:
            return COMM_APP_MODE_XDZ == p_params->app_mode
                       ? p_snapshot->battery.nominal_10mah / 10U
                       : p_snapshot->battery.nominal_10mah;
        case 41U:
        {
            int32_t temperature;
            if(0U == p_snapshot->battery.ntc_count)
            {
                return 0U;
            }
            /* Local register is signed degC*10; HIK cache remains unchanged. */
            temperature = (int32_t)
                              p_snapshot->battery.temperature_deci_kelvin[0U] -
                          2731L;
            return temperature <= 32767L ? (uint16_t)temperature : 0U;
        }
        case 42U:
            return p_snapshot->solar.voltage_10mv;
        case 43U:
            return p_snapshot->solar.current_10ma;
        default:
            return 0U; /* Register 33 is an idle one-shot command. */
    }
}

/* exported functions -------------------------------------------------------*/
uint8_t proto_local_is_boot_request(const uint8_t *p_data, uint16_t length)
{
    return NULL != p_data && BOOT_REQUEST_SIZE == length &&
           0 == memcmp(p_data, BOOT_REQUEST_TEXT, BOOT_REQUEST_SIZE);
}
uint16_t proto_local_crc16(const uint8_t *p_data, uint16_t length)
{
    uint16_t crc = 0xFFFFU;
    uint16_t index;
    uint8_t  bit;
    if(NULL == p_data)
    {
        return 0U;
    }
    for(index = 0U; index < length; index++)
    {
        crc ^= p_data[index];
        for(bit = 0U; bit < 8U; bit++)
        {
            crc = 0U != (crc & 1U) ? (crc >> 1U) ^ 0xA001U : crc >> 1U;
        }
    }
    return crc;
}
proto_local_result_t
proto_local_process(const uint8_t                *p_data,
                    uint16_t                      length,
                    const proto_local_snapshot_t *p_snapshot,
                    proto_local_command_t        *p_command,
                    uint8_t                      *p_output,
                    uint16_t                      capacity,
                    uint16_t                     *p_length)
{
    uint16_t start;
    uint16_t value;
    uint16_t index;
    uint16_t crc;
    uint16_t bytes;
    uint8_t  function;
    if(NULL != p_length)
    {
        *p_length = 0U;
    }
    if(NULL == p_data || 0U == length || PROTO_LOCAL_ADDRESS != p_data[0U])
    {
        return PROTO_LOCAL_NOT_LOCAL;
    }
    if(NULL == p_snapshot || NULL == p_command || NULL == p_output ||
       NULL == p_length || capacity < 5U || length < 8U || length > 256U)
    {
        return PROTO_LOCAL_DROP;
    }
    memset(p_command, 0, sizeof(*p_command));
    p_command->params = p_snapshot->params;
    crc               = proto_local_crc16(p_data, length - 2U);
    if((uint8_t)crc != p_data[length - 2U] ||
       (uint8_t)(crc >> 8U) != p_data[length - 1U])
    {
        return PROTO_LOCAL_DROP;
    }
    function = p_data[1U];
    start    = get_u16(p_data + 2U);
    value    = get_u16(p_data + 4U);
    if(3U == function || 1U == function)
    {
        if(8U != length || 0U == value || value > 125U)
        {
            return exception(function, 3U, p_output, p_length);
        }
        if(start < PROTO_LOCAL_FIRST_REGISTER ||
           start > (1U == function ? 20U : PROTO_LOCAL_LAST_REGISTER) ||
           value >
               (1U == function ? 21U : PROTO_LOCAL_LAST_REGISTER + 1U) - start)
        {
            return exception(function, 2U, p_output, p_length);
        }
        bytes = (uint16_t)(value * (1U == function ? 1U : 2U));
        if(capacity < bytes + 5U)
        {
            return PROTO_LOCAL_DROP;
        }
        p_output[0U] = PROTO_LOCAL_ADDRESS;
        p_output[1U] = function;
        p_output[2U] = (uint8_t)bytes;
        for(index = 0U; index < value; index++)
        {
            uint16_t reg = register_value(start + index, p_snapshot);
            if(1U == function)
            {
                p_output[3U + index] = (uint8_t)reg;
            }
            else
            {
                p_output[3U + 2U * index] = (uint8_t)(reg >> 8U);
                p_output[4U + 2U * index] = (uint8_t)reg;
            }
        }
        *p_length = bytes + 5U;
        append_crc(p_output, *p_length - 2U);
        return PROTO_LOCAL_REPLY;
    }
    if(5U == function)
    {
        if(8U != length)
        {
            return exception(function, 3U, p_output, p_length);
        }
        if(17U != start && 18U != start)
        {
            return exception(function, 2U, p_output, p_length);
        }
        if(0U != value && 0xFF00U != value)
        {
            return exception(function, 3U, p_output, p_length);
        }
        p_command->write_relay = 1U;
        p_command->relay       = (uint8_t)(start - 17U);
        p_command->level       = 0U == value ? 0U : 1U;
    }
    else if(6U == function)
    {
        if(8U != length)
        {
            return exception(function, 3U, p_output, p_length);
        }
        switch(start)
        {
            case 26U:
                p_command->params.uv_protect_mv = value;
                break;
            case 27U:
                p_command->params.uv_recover_mv = value;
                break;
            case 28U:
                if(value > 1U)
                {
                    return exception(function, 3U, p_output, p_length);
                }
                p_command->params.uv_enabled = (uint8_t)value;
                break;
            case 35U:
                if(value > 3U)
                {
                    return exception(function, 3U, p_output, p_length);
                }
                p_command->params.relay_contact_mask = (uint8_t)value;
                break;
            case 34U:
                if(value >= COMM_APP_MODE_NUM)
                {
                    return exception(function, 3U, p_output, p_length);
                }
                p_command->params.app_mode = (uint8_t)value;
                p_command->action          = PROTO_LOCAL_ACTION_MODE;
                break;
            case 33U:
                if(PROTO_LOCAL_SAVE_VALUE == value)
                {
                    p_command->action = PROTO_LOCAL_ACTION_SAVE;
                }
                else if(PROTO_LOCAL_DEFAULT_VALUE == value)
                {
                    p_command->action = PROTO_LOCAL_ACTION_DEFAULTS;
                }
                else
                {
                    return exception(function, 3U, p_output, p_length);
                }
                break;
            default:
                return exception(function, 2U, p_output, p_length);
        }
    }
    else if(0x10U == function)
    {
        if((2U != value && 4U != value) || length != 9U + 2U * value ||
           p_data[6U] != 2U * value)
        {
            return exception(function, 3U, p_output, p_length);
        }
        if(!((29U == start && (2U == value || 4U == value)) ||
             (31U == start && 2U == value)))
        {
            return exception(function, 2U, p_output, p_length);
        }
        if(29U == start)
        {
            p_command->params.rs485_1_baud = get_u32(p_data + 7U);
            if(4U == value)
            {
                p_command->params.rs485_2_baud = get_u32(p_data + 11U);
            }
        }
        else
        {
            p_command->params.rs485_2_baud = get_u32(p_data + 7U);
        }
    }
    else
    {
        return exception(function, 1U, p_output, p_length);
    }
    if(0U == comm_params_valid(&p_command->params))
    {
        return exception(function, 3U, p_output, p_length);
    }
    if(capacity < 8U)
    {
        return PROTO_LOCAL_DROP;
    }
    memcpy(p_output, p_data, 6U);
    append_crc(p_output, 6U);
    *p_length = 8U;
    return PROTO_LOCAL_REPLY;
}

/* end of file --------------------------------------------------------------*/
