/**
 ******************************************************************************
 *@file               :   app_hik.h
 *@brief              :   UART2 HIK host bring-up, shared controls and 4G setup.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef APP_HIK_H
#define APP_HIK_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/
typedef struct APP_HIK_STATUS_T
{
    uint32_t requests;
    uint32_t replies_queued;
    uint32_t invalid_frames;
    uint32_t unsupported_frames;
    uint32_t no_data;
    uint32_t ignored_frames;
    uint32_t blocked_attempts;
    uint32_t queue_timeouts;
    uint32_t maintenance_dropped;
    uint32_t configuration_requests;
} app_hik_status_t;

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
void    app_hik_init(void);
void    app_hik_discard_pending(void);
uint8_t app_hik_task(void);
uint8_t app_hik_get_status(app_hik_status_t *p_status);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* APP_HIK_H */
/* end of file --------------------------------------------------------------*/
