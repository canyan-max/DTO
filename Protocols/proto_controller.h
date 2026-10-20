/**
 ******************************************************************************
 *@file               :   proto_controller.h
 *@brief              :   Bounded legacy EVE/SZS query and measurement codec.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef PROTO_CONTROLLER_H
#define PROTO_CONTROLLER_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "telemetry_types.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/
typedef enum PROTO_CONTROLLER_PROFILE_T
{
    PROTO_CONTROLLER_EVE = 0U,
    PROTO_CONTROLLER_SZS,
    PROTO_CONTROLLER_NUM,
} proto_controller_profile_t;
typedef enum PROTO_CONTROLLER_RESULT_T
{
    PROTO_CONTROLLER_INVALID = 0U,
    PROTO_CONTROLLER_DATA,
    PROTO_CONTROLLER_EXCEPTION,
} proto_controller_result_t;

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
/* Step zero reads fixed settings; live steps are 1..5 (EVE), 1..2 (SZS).
 * Decode changes the supplied snapshots only for a complete, valid response. */
uint16_t proto_controller_query(proto_controller_profile_t profile,
                                uint8_t                    step,
                                uint8_t                   *p_out,
                                uint16_t                   capacity);
proto_controller_result_t
proto_controller_decode(proto_controller_profile_t profile,
                        uint8_t                    step,
                        const uint8_t             *p_data,
                        uint16_t                   length,
                        comm_battery_data_t       *p_battery,
                        comm_solar_data_t         *p_solar);
uint8_t proto_controller_next(proto_controller_profile_t profile, uint8_t step);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* PROTO_CONTROLLER_H */
/* end of file --------------------------------------------------------------*/
