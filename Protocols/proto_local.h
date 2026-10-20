/**
 ******************************************************************************
 *@file               :   proto_local.h
 *@brief              :   One local register protocol for all serial ports.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef PROTO_LOCAL_H
#define PROTO_LOCAL_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "comm_params.h"
#include "telemetry_types.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/
#define PROTO_LOCAL_ADDRESS (0x14U)
#define PROTO_LOCAL_SAVE_VALUE (56781U)
#define PROTO_LOCAL_DEFAULT_VALUE (52445U)
#define PROTO_LOCAL_FIRST_REGISTER (17U)
#define PROTO_LOCAL_LAST_REGISTER (43U)
#define PROTO_LOCAL_REGISTER_COUNT (27U)
#define PROTO_LOCAL_VERSION_SIZE (10U)
#define PROTO_LOCAL_MAX_REPLY (59U)

/* typedef ------------------------------------------------------------------*/
typedef enum PROTO_LOCAL_RESULT_T
{
    PROTO_LOCAL_NOT_LOCAL = 0U,
    PROTO_LOCAL_DROP,
    PROTO_LOCAL_REPLY,
} proto_local_result_t;
typedef enum PROTO_LOCAL_ACTION_T
{
    PROTO_LOCAL_ACTION_NONE = 0U,
    PROTO_LOCAL_ACTION_SAVE,
    PROTO_LOCAL_ACTION_DEFAULTS,
    PROTO_LOCAL_ACTION_MODE,
    PROTO_LOCAL_ACTION_BOOT,
} proto_local_action_t;
typedef struct PROTO_LOCAL_SNAPSHOT_T
{
    comm_params_t       params;
    uint8_t             relay_output[2U];
    uint8_t             version[PROTO_LOCAL_VERSION_SIZE];
    comm_battery_data_t battery;
    comm_solar_data_t   solar;
    uint8_t             battery_valid;
    uint8_t             solar_valid;
} proto_local_snapshot_t;
typedef struct PROTO_LOCAL_COMMAND_T
{
    comm_params_t        params;
    proto_local_action_t action;
    uint8_t              write_relay;
    uint8_t              relay;
    uint8_t              level;
} proto_local_command_t;

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
/* Exact 8-byte ASCII BootLoad, without CRC or line endings. */
uint8_t  proto_local_is_boot_request(const uint8_t *p_data, uint16_t length);
uint16_t proto_local_crc16(const uint8_t *p_data, uint16_t length);
/* Pure codec. Reads span 17..43; legacy 01 keeps one byte per relay alias.
 * Only a non-exception REPLY authorizes applying the proposed command.
 * Insufficient output space returns DROP; the largest reply needs 59 bytes. */
proto_local_result_t
proto_local_process(const uint8_t                *p_data,
                    uint16_t                      length,
                    const proto_local_snapshot_t *p_snapshot,
                    proto_local_command_t        *p_command,
                    uint8_t                      *p_output,
                    uint16_t                      capacity,
                    uint16_t                     *p_length);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* PROTO_LOCAL_H */
/* end of file --------------------------------------------------------------*/
