/**
 ******************************************************************************
 *@file               :   app_local.h
 *@brief              :   Local parameter replies and deferred save maintenance.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef APP_LOCAL_H
#define APP_LOCAL_H

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
typedef enum APP_LOCAL_RESULT_T
{
    APP_LOCAL_NOT_LOCAL = 0U,
    APP_LOCAL_CONSUMED,
    APP_LOCAL_RETRY,
    APP_LOCAL_ERROR,
} app_local_result_t;
typedef enum APP_LOCAL_STATE_T
{
    APP_LOCAL_IDLE = 0U,
    APP_LOCAL_REPLY_PENDING,
    APP_LOCAL_WAIT_TX,
    APP_LOCAL_DONE,
    APP_LOCAL_FAILED,
    APP_LOCAL_RESET_REQUESTED,
} app_local_state_t;
typedef struct APP_LOCAL_STATUS_T
{
    app_local_state_t state;
    uint32_t          requests;
    uint32_t          dropped;
    uint32_t          failures;
    uint32_t          saves;
} app_local_status_t;

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
/* All three ports share the same local address and reply on their source. */
app_local_result_t
app_local_accept(comm_port_t source, const uint8_t *p_data, uint16_t length);
uint8_t app_local_busy(void);
uint8_t app_local_maintenance_pending(void);
uint8_t app_local_task(void);
uint8_t app_local_get_status(app_local_status_t *p_status);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* APP_LOCAL_H */
/* end of file --------------------------------------------------------------*/
