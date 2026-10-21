/**
 ******************************************************************************
 *@file               :   svc_relay.h
 *@brief              :   Two GPIO relays, remembered requests and UV override.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef SVC_RELAY_H
#define SVC_RELAY_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "dto_config.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/
typedef struct SVC_RELAY_STATUS_T
{
    uint8_t requested[DTO_RELAY_COUNT];
    uint8_t output[DTO_RELAY_COUNT]; /* effective logical state after UV */
    uint8_t
        gpio[DTO_RELAY_COUNT]; /* physical pin level after contact mapping */
    uint8_t  from_flash;
    uint8_t  dirty;
    uint8_t  uv_active;
    uint8_t  sample_invalid;
    uint32_t saves;
    uint32_t failures;
    uint32_t protection_entries;
    uint32_t protection_recoveries;
} svc_relay_status_t;

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
/* Startup only: decode/cache memory and return GPIO bits, without driving IO.
 */
uint8_t svc_relay_prepare_startup(uint8_t *p_gpio_mask);
uint8_t svc_relay_init(void);
uint8_t svc_relay_set(uint8_t relay, uint8_t level);
uint8_t svc_relay_task(void);
/* Caller drains TX, suspends RX and pauses routing before Flash access. */
uint8_t svc_relay_save(void);
uint8_t svc_relay_get_status(svc_relay_status_t *p_status);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_RELAY_H */
/* end of file --------------------------------------------------------------*/
