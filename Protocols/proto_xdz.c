/**
 ******************************************************************************
 *@file               :   proto_xdz.c
 *@brief              :   Bounded XDZ master frames and legacy host register mapping.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include <string.h>
#include "proto_xdz.h"
#include "proto_local.h"
#include "xdz_config.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* private  functions  ------------------------------------------------------*/

static uint16_t get_u16(const uint8_t *p_data)
{
    return ((uint16_t)p_data[0U] << 8U) | p_data[1U];
}
static void append_crc(uint8_t *p_data, uint16_t length)
{
    uint16_t crc        = proto_local_crc16(p_data, length);
    p_data[length]      = (uint8_t)crc;
    p_data[length + 1U] = (uint8_t)(crc >> 8U);
}
static uint8_t valid_crc(const uint8_t *p_data, uint16_t length)
{
    return proto_local_crc16(p_data, (uint16_t)(length - 2U)) ==
           ((uint16_t)p_data[length - 1U] << 8U | p_data[length - 2U]);
}
static proto_xdz_host_result_t
exception(uint8_t function, uint8_t *p_output, uint16_t *p_length)
{
    p_output[0U] = XDZ_HOST_ADDRESS;
    p_output[1U] = function | 0x80U;
    p_output[2U] = 0x44U;
    append_crc(p_output, 3U);
    *p_length = 5U;
    return PROTO_XDZ_HOST_REPLY;
}
static uint16_t protect1(const proto_xdz_data_t *p_data)
{
    uint16_t flags  = p_data->protection;
    uint16_t result = 0U;
    if(0U != (flags & 0x0050U))
    {
        result |= 0x0021U;
    }
    if(0U != (flags & 0x0700U))
    {
        result |= 0x0002U;
    }
    if(0U != (flags & 0x0100U))
    {
        result |= 0x0004U;
    }
    if(0U != (flags & 0x0005U))
    {
        result |= 0x0008U;
    }
    if(0U != (flags & 0x000AU) || p_data->voltage_10mv / 10U <= 250U)
    {
        result |= 0x0010U;
    }
    if(0U != (flags & 0x0200U))
    {
        result |= 0x0040U;
    }
    if(0U != (flags & 0x0002U) || p_data->voltage_10mv / 10U <= 250U)
    {
        result |= 0x0080U;
    }
    if(0U != (flags & 0x0020U))
    {
        result |= 0x0200U;
    }
    if(0U != (flags & 0x0080U))
    {
        result |= 0x0400U;
    }
    if(0U != (flags & 0x0400U))
    {
        result |= 0x1000U;
    }
    return result;
}
static uint16_t host_register(uint16_t address, const proto_xdz_data_t *p_data)
{
    int32_t  current      = p_data->current_10ma / 10;
    uint16_t voltage      = p_data->voltage_10mv / 10U;
    int32_t  temperature1 = ((int32_t)p_data->temperature_deci_kelvin[0U] -
                             2731) /
                            10;
    int32_t  temperature2 = ((int32_t)p_data->temperature_deci_kelvin[1U] -
                             2731) /
                            10;
    switch(address)
    {
        case 0x0200U:
            return (uint16_t)(((p_data->fet_state & 1U) << 7U) |
                              ((p_data->fet_state & 2U) << 5U));
        case 0x0201U:
            return protect1(p_data);
        case 0x0202U:
            return p_data->soc_percent <= 30U ? 0xF401U : 0U;
        case 0x0203U:
            return p_data->remaining_deci_ah / 10U;
        case 0x0204U:
            return (uint16_t)p_data->soc_percent << 8U;
        case 0x0205U:
            if(current < 0)
            {
                current = -current;
            }
            return (uint16_t)(((uint32_t)(voltage / 10U) * (uint32_t)current) /
                              10U);
        case 0x0206U:
            return voltage;
        case 0x0207U:
            return (uint16_t)current;
        case 0x020AU:
            return (uint16_t)(((uint16_t)(uint8_t)temperature1 << 8U) |
                              (uint8_t)temperature2);
        case 0x0217U:
        case 0x0218U:
            /* Preserve reference formula and wire width, not an extra ADC. */
            return (uint16_t)((uint32_t)p_data->voltage_10mv * 100U / 8U);
        default:
            return address >= 0x020BU && address <= 0x0211U ? 0xCECEU : 0U;
    }
}

/* exported functions -------------------------------------------------------*/

uint16_t proto_xdz_query(uint8_t step, uint8_t *p_output, uint16_t capacity)
{
    uint16_t start = 0U == step ? 0x0012U : 0x0025U;
    if(NULL == p_output || capacity < 8U || step > 1U)
    {
        return 0U;
    }
    p_output[0U] = XDZ_DEVICE_ADDRESS;
    p_output[1U] = 3U;
    p_output[2U] = (uint8_t)(start >> 8U);
    p_output[3U] = (uint8_t)start;
    p_output[4U] = 0U;
    p_output[5U] = 0U == step ? 3U : 6U;
    append_crc(p_output, 6U);
    return 8U;
}
proto_xdz_result_t proto_xdz_decode(uint8_t           step,
                                    const uint8_t    *p_data,
                                    uint16_t          length,
                                    proto_xdz_data_t *p_snapshot)
{
    uint8_t bytes = 0U == step ? 6U : 12U;
    if(NULL == p_data || NULL == p_snapshot || step > 1U || length < 5U ||
       XDZ_DEVICE_ADDRESS != p_data[0U] || 0U == valid_crc(p_data, length))
    {
        return PROTO_XDZ_INVALID;
    }
    if(5U == length && 0x83U == p_data[1U])
    {
        return PROTO_XDZ_EXCEPTION;
    }
    if(length != (uint16_t)(bytes + 5U) || 3U != p_data[1U] ||
       bytes != p_data[2U])
    {
        return PROTO_XDZ_INVALID;
    }
    if(0U == step)
    {
        p_snapshot->voltage_10mv      = get_u16(p_data + 3U);
        p_snapshot->current_10ma      = (int16_t)get_u16(p_data + 5U);
        p_snapshot->remaining_deci_ah = get_u16(p_data + 7U);
    }
    else
    {
        if(get_u16(p_data + 9U) > 100U)
        {
            return PROTO_XDZ_INVALID;
        }
        p_snapshot->temperature_deci_kelvin[0U] = get_u16(p_data + 3U);
        p_snapshot->temperature_deci_kelvin[1U] = get_u16(p_data + 5U);
        p_snapshot->nominal_deci_ah             = get_u16(p_data + 7U);
        p_snapshot->soc_percent                 = (uint8_t)get_u16(p_data + 9U);
        p_snapshot->fet_state  = (uint8_t)(get_u16(p_data + 11U) & 3U);
        p_snapshot->protection = get_u16(p_data + 13U);
    }
    return PROTO_XDZ_DATA;
}
proto_xdz_host_result_t
proto_xdz_host_process(const uint8_t          *p_data,
                       uint16_t                length,
                       const proto_xdz_data_t *p_snapshot,
                       uint8_t                *p_relay_level,
                       uint8_t                *p_output,
                       uint16_t                capacity,
                       uint16_t               *p_length)
{
    uint16_t start, count, index;
    uint8_t  function;
    if(NULL == p_length)
    {
        return PROTO_XDZ_HOST_DROP;
    }
    *p_length = 0U;
    if(NULL == p_data || NULL == p_snapshot || NULL == p_output ||
       NULL == p_relay_level || length < 4U || capacity < 5U ||
       XDZ_HOST_ADDRESS != p_data[0U] || 0U == valid_crc(p_data, length))
    {
        return PROTO_XDZ_HOST_DROP;
    }
    function = p_data[1U];
    if(4U != function && 0x10U != function)
    {
        return PROTO_XDZ_HOST_DROP;
    }
    if(length < 8U)
    {
        return exception(function, p_output, p_length);
    }
    start = get_u16(p_data + 2U);
    count = get_u16(p_data + 4U);
    if(4U == function)
    {
        if(8U != length || start < XDZ_FIRST_REGISTER ||
           start > XDZ_LAST_REGISTER || 0U == count ||
           count > (uint16_t)(XDZ_LAST_REGISTER - start + 1U))
        {
            return exception(function, p_output, p_length);
        }
        if(capacity < (uint16_t)(count * 2U + 5U))
        {
            return PROTO_XDZ_HOST_DROP;
        }
        p_output[0U] = XDZ_HOST_ADDRESS;
        p_output[1U] = 4U;
        p_output[2U] = (uint8_t)(count * 2U);
        for(index = 0U; index < count; index++)
        {
            uint16_t value            = host_register((uint16_t)(start + index),
                                                      p_snapshot);
            p_output[3U + index * 2U] = (uint8_t)(value >> 8U);
            p_output[4U + index * 2U] = (uint8_t)value;
        }
        *p_length = (uint16_t)(count * 2U + 5U);
        append_crc(p_output, (uint16_t)(*p_length - 2U));
        return PROTO_XDZ_HOST_REPLY;
    }
    if(11U != length || XDZ_RELAY_REGISTER != start || 1U != count ||
       2U != p_data[6U])
    {
        return exception(function, p_output, p_length);
    }
    if(capacity < 8U)
    {
        return PROTO_XDZ_HOST_DROP;
    }
    /* Legacy XDZ 0425 is active-low: zero requests HIGH, nonzero LOW.
     * Convert at the protocol boundary; common inversion/UV still apply. */
    *p_relay_level = 0U == get_u16(p_data + 7U) ? 1U : 0U;
    memcpy(p_output, p_data, 6U);
    append_crc(p_output, 6U);
    *p_length = 8U;
    return PROTO_XDZ_HOST_RELAY;
}

/* end of file --------------------------------------------------------------*/
