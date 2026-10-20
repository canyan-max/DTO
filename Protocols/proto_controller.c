/**
 ******************************************************************************
 *@file               :   proto_controller.c
 *@brief              :   Checked legacy EVE/SZS queries and integer measurements.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "proto_controller.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/
typedef struct CONTROLLER_QUERY_T
{
    uint8_t  function;
    uint16_t address;
    uint16_t count;
} controller_query_t;

/* variables ----------------------------------------------------------------*/
static const controller_query_t g_eve[] = {
    {3U, 0x9001U, 14U}, {4U, 0x3100U, 2U}, {4U, 0x3110U, 1U},
    {4U, 0x311AU, 1U},  {4U, 0x3200U, 2U}, {4U, 0x331AU, 3U},
};
static const controller_query_t g_szs[] = {
    {3U, 0xE002U, 10U},
    {3U, 0x0100U, 9U},
    {3U, 0x00FDU, 1U},
};

/* private  functions  ------------------------------------------------------*/
static const controller_query_t *query(proto_controller_profile_t profile,
                                       uint8_t                    step)
{
    if(PROTO_CONTROLLER_EVE == profile &&
       step < sizeof(g_eve) / sizeof(g_eve[0U]))
    {
        return &g_eve[step];
    }
    if(PROTO_CONTROLLER_SZS == profile &&
       step < sizeof(g_szs) / sizeof(g_szs[0U]))
    {
        return &g_szs[step];
    }
    return NULL;
}
static uint16_t crc16(const uint8_t *p_data, uint16_t length)
{
    uint16_t crc = 0xFFFFU;
    uint16_t i;
    uint8_t  bit;
    for(i = 0U; i < length; i++)
    {
        crc ^= p_data[i];
        for(bit = 0U; bit < 8U; bit++)
        {
            crc = 0U != (crc & 1U) ? (crc >> 1U) ^ 0xA001U : crc >> 1U;
        }
    }
    return crc;
}
static uint16_t word(const uint8_t *p_data, uint8_t index)
{
    return ((uint16_t)p_data[2U * index] << 8U) | p_data[2U * index + 1U];
}
static int32_t signed_word(uint16_t value)
{
    return value < 0x8000U ? (int32_t)value : (int32_t)value - 65536L;
}
static int32_t signed_byte(uint8_t value)
{
    return value < 0x80U ? (int32_t)value : (int32_t)value - 256L;
}
static uint8_t decode_data(proto_controller_profile_t profile,
                           uint8_t                    step,
                           const uint8_t             *p_regs,
                           comm_battery_data_t       *p_battery,
                           comm_solar_data_t         *p_solar)
{
    uint32_t value;
    int32_t  current;
    int32_t  temperature;
    if(0U == step)
    {
        value = (uint32_t)word(p_regs, 0U) * 100U;
        if(value > 65535U)
        {
            return 0U;
        }
        p_battery->nominal_10mah = (uint16_t)value;
        return 1U;
    }
    if(PROTO_CONTROLLER_EVE == profile)
    {
        switch(step)
        {
            case 1U:
                p_solar->number       = 1U;
                p_solar->voltage_10mv = word(p_regs, 0U);
                p_solar->current_10ma = word(p_regs, 1U);
                break;
            case 2U:
                temperature = signed_word(word(p_regs, 0U));
                /* Values below absolute zero cannot be represented by HIK. */
                if(temperature < -27310L)
                {
                    return 0U;
                }
                temperature          = (temperature + 27310L) / 10L;
                p_battery->ntc_count = 1U;
                p_battery->temperature_deci_kelvin[0U] = (uint16_t)temperature;
                break;
            case 3U:
                if(word(p_regs, 0U) > 100U)
                {
                    return 0U;
                }
                p_battery->soc_percent = (uint8_t)word(p_regs, 0U);
                break;
            case 4U:
                /* Keep the legacy HIK mapping: controller status bits are not
                 * BMS protection bits. The query is retained for compatibility.
                 */
                break;
            case 5U:
                /* 331B low word, 331C signed high word. Do not add the words
                 * as the old implementation did (negative values were wrong).
                 */
                current = signed_word(word(p_regs, 2U)) * 65536L +
                          word(p_regs, 1U);
                if(current < -32768L || current > 32767L)
                {
                    return 0U;
                }
                p_battery->voltage_10mv = word(p_regs, 0U);
                p_battery->current_10ma = (int16_t)current;
                p_battery->fet_state    = 0L == current ? 0U : 3U;
                break;
            default:
                return 0U;
        }
    }
    else if(1U == step)
    {
        if(word(p_regs, 0U) > 100U || word(p_regs, 1U) > 6553U ||
           word(p_regs, 7U) > 6553U)
        {
            return 0U;
        }
        current = signed_word(word(p_regs, 2U));
        if(0L == current)
        {
            current = -(int32_t)word(p_regs, 5U);
        }
        if(current < -32768L || current > 32767L)
        {
            return 0U;
        }
        p_battery->soc_percent                 = (uint8_t)word(p_regs, 0U);
        p_battery->voltage_10mv                = word(p_regs, 1U) * 10U;
        p_battery->current_10ma                = (int16_t)current;
        p_battery->fet_state                   = 0L == current ? 0U : 3U;
        p_battery->ntc_count                   = 2U;
        value                                  = word(p_regs, 3U);
        p_battery->temperature_deci_kelvin[0U] = (uint16_t)(signed_byte((
                                                                uint8_t)value) *
                                                                10L +
                                                            2731L);
        p_battery
            ->temperature_deci_kelvin[1U] = (uint16_t)(signed_byte(
                                                           (uint8_t)(value >>
                                                                     8U)) *
                                                           10L +
                                                       2731L);
        p_solar->number                   = 1U;
        p_solar->voltage_10mv             = word(p_regs, 7U) * 10U;
        p_solar->current_10ma             = word(p_regs, 8U);
    }
    /* SZS 00FD returns ONE register. Never read a nonexistent second word.
     * HIK historically sends zero protection and derives FET from current. */
    p_battery->software_version = 0x10U;
    return 1U;
}

/* exported functions -------------------------------------------------------*/
uint16_t proto_controller_query(proto_controller_profile_t profile,
                                uint8_t                    step,
                                uint8_t                   *p_out,
                                uint16_t                   capacity)
{
    const controller_query_t *p_query = query(profile, step);
    uint16_t                  crc;
    if(NULL == p_query || NULL == p_out || capacity < 8U)
    {
        return 0U;
    }
    p_out[0U] = 1U;
    p_out[1U] = p_query->function;
    p_out[2U] = (uint8_t)(p_query->address >> 8U);
    p_out[3U] = (uint8_t)p_query->address;
    p_out[4U] = (uint8_t)(p_query->count >> 8U);
    p_out[5U] = (uint8_t)p_query->count;
    crc       = crc16(p_out, 6U);
    p_out[6U] = (uint8_t)crc;
    p_out[7U] = (uint8_t)(crc >> 8U);
    return 8U;
}
uint8_t proto_controller_next(proto_controller_profile_t profile, uint8_t step)
{
    return (PROTO_CONTROLLER_EVE == profile && step < 5U) ||
                   (PROTO_CONTROLLER_SZS == profile && step < 2U)
               ? step + 1U
               : 1U;
}
proto_controller_result_t
proto_controller_decode(proto_controller_profile_t profile,
                        uint8_t                    step,
                        const uint8_t             *p_data,
                        uint16_t                   length,
                        comm_battery_data_t       *p_battery,
                        comm_solar_data_t         *p_solar)
{
    const controller_query_t *p_query = query(profile, step);
    comm_battery_data_t       battery;
    comm_solar_data_t         solar;
    uint16_t                  crc;
    if(NULL == p_query || NULL == p_data || NULL == p_battery ||
       NULL == p_solar || length < 5U || 1U != p_data[0U] ||
       (5U != length && length != 5U + 2U * p_query->count))
    {
        return PROTO_CONTROLLER_INVALID;
    }
    crc = crc16(p_data, length - 2U);
    if((uint8_t)crc != p_data[length - 2U] ||
       (uint8_t)(crc >> 8U) != p_data[length - 1U])
    {
        return PROTO_CONTROLLER_INVALID;
    }
    if(5U == length && (p_query->function | 0x80U) == p_data[1U])
    {
        return PROTO_CONTROLLER_EXCEPTION;
    }
    if(p_query->function != p_data[1U] || length != 5U + 2U * p_query->count ||
       p_data[2U] != 2U * p_query->count)
    {
        return PROTO_CONTROLLER_INVALID;
    }
    battery = *p_battery;
    solar   = *p_solar;
    if(0U == decode_data(profile, step, p_data + 3U, &battery, &solar))
    {
        return PROTO_CONTROLLER_INVALID;
    }
    *p_battery = battery;
    *p_solar   = solar;
    return PROTO_CONTROLLER_DATA;
}

/* end of file --------------------------------------------------------------*/
