/**
 ******************************************************************************
 *@file               :   svc_gm800.h
 *@brief              :   GM800 setup with bounded per-command acknowledgement.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef SVC_GM800_H
#define SVC_GM800_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "comm_config.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/
typedef enum SVC_GM800_STATE_T
{
    SVC_GM800_IDLE = 0U,
    SVC_GM800_DRAINING,
    SVC_GM800_SENDING,
    SVC_GM800_SENT_UNVERIFIED,
    SVC_GM800_TRANSPORT_ERROR,
    SVC_GM800_TIMEOUT,
    SVC_GM800_FINISHING,
    SVC_GM800_CONFIRMED,
    SVC_GM800_RESPONSE_ERROR,
    SVC_GM800_RESPONSE_TIMEOUT,
} svc_gm800_state_t;
typedef struct SVC_GM800_STATUS_T
{
    svc_gm800_state_t state;
    uint32_t          sessions_started;
    uint32_t          sequences_sent;
    uint32_t          failures;
    uint32_t          module_rx_frames;
    uint32_t          suppressed_frames;
    uint32_t          log_frames;
    uint32_t          log_dropped;
    uint8_t           commands_sent;
    uint8_t           commands_confirmed;
    uint8_t           failed_step; /* 1..23; zero when no command was issued. */
} svc_gm800_status_t;

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
/* Main-context API. Identity is exactly 20 digits; capacity includes NUL. */
/* Raw module replies are logged to the initiating RS485 port, best effort. */
uint8_t
svc_gm800_start(comm_port_t source, const char *p_identity, uint16_t capacity);
uint8_t svc_gm800_busy(void);
uint8_t svc_gm800_task(void);
uint8_t svc_gm800_get_status(svc_gm800_status_t *p_status);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_GM800_H */
/* end of file --------------------------------------------------------------*/
