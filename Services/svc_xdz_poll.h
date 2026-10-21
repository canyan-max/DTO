/**
 ******************************************************************************
 *@file               :   svc_xdz_poll.h
 *@brief              :   Two-step XDZ acquisition and validated shared telemetry.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef SVC_XDZ_POLL_H
#define SVC_XDZ_POLL_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "proto_xdz.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

typedef enum SVC_XDZ_POLL_STATE_T
{
    SVC_XDZ_POLL_QUIET = 0U,
    SVC_XDZ_POLL_QUEUED,
    SVC_XDZ_POLL_WAITING,
    SVC_XDZ_POLL_PAUSED,
} svc_xdz_poll_state_t;
typedef struct SVC_XDZ_POLL_STATUS_T
{
    svc_xdz_poll_state_t state;
    uint8_t              step;
    uint8_t              parts;
    uint32_t             sent;
    uint32_t             received;
    uint32_t             invalid;
    uint32_t             exceptions;
    uint32_t             timeouts;
    uint32_t             tx_failures;
    uint32_t             updates;
} svc_xdz_poll_status_t;
/* variables ----------------------------------------------------------------*/

extern svc_xdz_poll_status_t g_xdz_poll;
/* function  ----------------------------------------------------------------*/
void    svc_xdz_poll_init(void);
void    svc_xdz_poll_pause(void);
uint8_t svc_xdz_poll_task(void);
void    svc_xdz_poll_accept(const uint8_t *p_data, uint16_t length);
/* Invalid/expired data returns zero measurements (0 C) and valid=0. */
uint8_t svc_xdz_poll_get(proto_xdz_data_t *p_data, uint8_t *p_valid);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_XDZ_POLL_H */
/* end of file --------------------------------------------------------------*/
