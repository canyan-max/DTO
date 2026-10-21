/**
 ******************************************************************************
 *@file               :   proto_xdz.h
 *@brief              :   XDZ battery master and FKY-compatible host codec.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef PROTO_XDZ_H
#define PROTO_XDZ_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

typedef struct PROTO_XDZ_DATA_T
{
    uint16_t voltage_10mv;
    int16_t  current_10ma;
    uint16_t remaining_deci_ah;
    uint16_t nominal_deci_ah;
    uint16_t temperature_deci_kelvin[2U];
    uint16_t protection;
    uint8_t  soc_percent;
    uint8_t  fet_state;
} proto_xdz_data_t;
typedef enum PROTO_XDZ_RESULT_T
{
    PROTO_XDZ_INVALID = 0U,
    PROTO_XDZ_DATA,
    PROTO_XDZ_EXCEPTION,
} proto_xdz_result_t;
typedef enum PROTO_XDZ_HOST_RESULT_T
{
    PROTO_XDZ_HOST_DROP = 0U,
    PROTO_XDZ_HOST_REPLY,
    PROTO_XDZ_HOST_RELAY,
} proto_xdz_host_result_t;
/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
uint16_t proto_xdz_query(uint8_t step, uint8_t *p_output, uint16_t capacity);
proto_xdz_result_t proto_xdz_decode(uint8_t           step,
                                    const uint8_t    *p_data,
                                    uint16_t          length,
                                    proto_xdz_data_t *p_snapshot);
/* Pure codec: RELAY authorizes both relays. Legacy 0425 zero maps to logical
 * level 1, nonzero to level 0; shared inversion/UV follow this conversion.
 * Bad CRC/short input drops; valid illegal ranges return legacy 0x44 error. */
proto_xdz_host_result_t
proto_xdz_host_process(const uint8_t          *p_data,
                       uint16_t                length,
                       const proto_xdz_data_t *p_snapshot,
                       uint8_t                *p_relay_level,
                       uint8_t                *p_output,
                       uint16_t                capacity,
                       uint16_t               *p_length);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* PROTO_XDZ_H */
/* end of file --------------------------------------------------------------*/
