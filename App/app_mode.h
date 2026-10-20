/**
 ******************************************************************************
 *@file               :   app_mode.h
 *@brief              :   Exclusive runtime DTO and HIK mode ownership.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef APP_MODE_H
#define APP_MODE_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "comm_params.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/
typedef enum APP_MODE_STATE_T
{
    APP_MODE_ACTIVE = 0U,
    APP_MODE_DRAINING,
    APP_MODE_QUIET,
} app_mode_state_t;
typedef struct APP_MODE_STATUS_T
{
    comm_app_mode_t  active;
    comm_app_mode_t  requested;
    app_mode_state_t state;
    uint32_t         switches;
    uint32_t         failures;
} app_mode_status_t;

/* variables ----------------------------------------------------------------*/
extern app_mode_status_t g_mode;

/* function  ----------------------------------------------------------------*/
/* Startup only. Mode transitions never initialize shared relay/UV/SN services.
 */
uint8_t app_mode_init(comm_app_mode_t initial);
/* Called after local ACK and all prior TX complete. No Flash writes. */
uint8_t app_mode_request(comm_app_mode_t mode);
uint8_t app_mode_task(void);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* APP_MODE_H */
/* end of file --------------------------------------------------------------*/
