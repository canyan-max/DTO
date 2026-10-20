/**
 ******************************************************************************
 *@file               :   svc_poll.h
 *@brief              :   Controller transactions and telemetry publishing.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef SVC_POLL_H
#define SVC_POLL_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "proto_controller.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/
typedef enum SVC_POLL_STATE_T
{
    SVC_POLL_QUIET = 0U,
    SVC_POLL_READY,
    SVC_POLL_QUEUED,
    SVC_POLL_WAITING,
    SVC_POLL_PAUSED,
} svc_poll_state_t;
typedef struct SVC_POLL_STATUS_T
{
    proto_controller_profile_t profile;
    svc_poll_state_t           state;
    uint8_t                    step;
    uint8_t                    battery_parts;
    uint32_t                   sent;
    uint32_t                   received;
    uint32_t                   invalid;
    uint32_t                   exceptions;
    uint32_t                   timeouts;
    uint32_t                   tx_failures;
    uint32_t                   switches;
    uint32_t                   battery_updates;
    uint32_t                   solar_updates;
} svc_poll_status_t;

/* variables ----------------------------------------------------------------*/
/* Read-only debug access; application logic uses the telemetry service. */
extern svc_poll_status_t g_poll;

/* function  ----------------------------------------------------------------*/
/* Main context only. App owns RX and feeds complete device-port frames here.
 * Pause during exclusive configuration/Flash maintenance, then task resumes
 * after a fresh quiet gap. No relay, identity or parameter initialization. */
void    svc_poll_init(void);
void    svc_poll_pause(void);
uint8_t svc_poll_task(void);
void    svc_poll_accept(const uint8_t *p_data, uint16_t length);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_POLL_H */
/* end of file --------------------------------------------------------------*/
