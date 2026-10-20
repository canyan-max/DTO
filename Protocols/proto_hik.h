/**
 ******************************************************************************
 *@file               :   proto_hik.h
 *@brief              :   Legacy DD/A5 HIK read framing and payload encoding.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef PROTO_HIK_H
#define PROTO_HIK_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "telemetry_types.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/
#define PROTO_HIK_BATTERY (0x03U)
#define PROTO_HIK_SOLAR (0x0FU)
#define PROTO_HIK_INFO (0x0CU)
#define PROTO_HIK_SN_READ (0x0DU)
#define PROTO_HIK_SN_WRITE (0xA3U)
#define PROTO_HIK_SN_MAX (16U)
#define PROTO_HIK_MAX_REPLY (40U)

/* typedef ------------------------------------------------------------------*/
typedef enum PROTO_HIK_RESULT_T
{
    PROTO_HIK_NOT_HIK = 0U,
    PROTO_HIK_INVALID,
    PROTO_HIK_UNSUPPORTED,
    PROTO_HIK_READ,
    PROTO_HIK_WRITE_SN,
} proto_hik_result_t;

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
/* Two's-complement byte sum. Request checksum covers command/length/payload;
 * response checksum covers status/length/payload, excluding response command.
 */
uint16_t proto_hik_checksum(const uint8_t *p_data, uint16_t length);
proto_hik_result_t
proto_hik_parse(const uint8_t *p_data, uint16_t length, uint8_t *p_command);
uint16_t proto_hik_build_battery(const comm_battery_data_t *p_data,
                                 uint8_t                   *p_output,
                                 uint16_t                   capacity);
uint16_t proto_hik_build_solar(const comm_solar_data_t *p_data,
                               uint8_t                 *p_output,
                               uint16_t                 capacity);
uint16_t proto_hik_build_info(uint8_t *p_output, uint16_t capacity);
uint16_t proto_hik_build_sn(const uint8_t *p_sn,
                            uint16_t       length,
                            uint8_t       *p_output,
                            uint16_t       capacity);
/* User-confirmed seven-byte A3 acknowledgment: DD A3 00 00 00 00 77. */
uint16_t proto_hik_build_sn_ack(uint8_t *p_output, uint16_t capacity);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* PROTO_HIK_H */
/* end of file --------------------------------------------------------------*/
