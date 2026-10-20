/**
 ******************************************************************************
 *@file               :   telemetry_types.h
 *@brief              :   External battery and solar measurements in fixed units.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef TELEMETRY_TYPES_H
#define TELEMETRY_TYPES_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/
#define COMM_TELEMETRY_NTC_MAX (5U)

/* typedef ------------------------------------------------------------------*/
typedef struct COMM_BATTERY_DATA_T
{
    uint16_t voltage_10mv;
    int16_t  current_10ma;
    uint16_t remaining_10mah;
    uint16_t nominal_10mah;
    uint16_t cycle_count;
    uint16_t manufacture_date;
    uint16_t balance_low;
    uint16_t balance_high;
    uint16_t protection;
    uint8_t  software_version;
    uint8_t  soc_percent;
    uint8_t  fet_state;
    uint8_t  cell_count;
    uint8_t  ntc_count;
    /* Legacy HIK temperature representation: degC * 10 + 2731. */
    uint16_t temperature_deci_kelvin[COMM_TELEMETRY_NTC_MAX];
} comm_battery_data_t;
typedef struct COMM_SOLAR_DATA_T
{
    uint8_t  number;
    uint16_t voltage_10mv;
    uint16_t current_10ma;
} comm_solar_data_t;

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* TELEMETRY_TYPES_H */
/* end of file --------------------------------------------------------------*/
