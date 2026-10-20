/**
 ******************************************************************************
 *@file               :   proto_gm800.h
 *@brief              :   Bounded legacy CLOUD request parsing and AT command encoding.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef PROTO_GM800_H
#define PROTO_GM800_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/
#define PROTO_GM800_ID_SIZE (20U)
#define PROTO_GM800_COMMAND_COUNT (23U)
/* Zero-based indices after CLOUD, before save/restart. */
#define PROTO_GM800_STEP_ICCID (18U)
#define PROTO_GM800_STEP_IMEI (19U)
#define PROTO_GM800_STEP_SAVE_FIRST (20U)
#define PROTO_GM800_STEP_SAVE_SECOND (21U)
#define PROTO_GM800_STEP_RESTART (22U)
#define PROTO_GM800_RESPONSE_LINE_SIZE (128U)
#define PROTO_GM800_RESPONSE_OK (1U)
#define PROTO_GM800_RESPONSE_ERROR (2U)

/* typedef ------------------------------------------------------------------*/
typedef enum PROTO_GM800_REQUEST_T
{
    PROTO_GM800_NOT_REQUEST = 0U,
    PROTO_GM800_VALID_REQUEST,
    PROTO_GM800_INVALID_REQUEST,
} proto_gm800_request_t;

typedef struct PROTO_GM800_RESPONSE_T
{
    char     line[PROTO_GM800_RESPONSE_LINE_SIZE];
    uint16_t length;
    uint8_t  overflow;
} proto_gm800_response_t;

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
/* p_identity requires 21 bytes; legacy optional suffix is ignored. */
proto_gm800_request_t proto_gm800_parse_request(const uint8_t *p_data,
                                                uint16_t       length,
                                                char          *p_identity,
                                                uint16_t       capacity);
/* Streaming parser: handshake step 0 accepts 'a', step 1 accepts +ok.
 * Ordinary steps require a complete OK line; echoes never acknowledge. */
void    proto_gm800_response_reset(proto_gm800_response_t *p_response);
uint8_t proto_gm800_response_feed(proto_gm800_response_t *p_response,
                                  uint8_t                 step,
                                  const uint8_t          *p_data,
                                  uint16_t                length);
/* Step is zero based. Includes CRLF where present in the original profile. */
uint8_t proto_gm800_command(uint8_t     step,
                            const char *p_identity,
                            uint8_t    *p_output,
                            uint16_t    capacity,
                            uint16_t   *p_length);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* PROTO_GM800_H */
/* end of file --------------------------------------------------------------*/
