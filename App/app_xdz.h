/**
 ******************************************************************************
 *@file               :   app_xdz.h
 *@brief              :   XDZ host on UART3 and battery acquisition on UART2.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef APP_XDZ_H
#define APP_XDZ_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

typedef struct APP_XDZ_STATUS_T
{
    uint32_t requests;
    uint32_t replies_queued;
    uint32_t relay_commands;
    uint32_t dropped;
    uint32_t no_data;
    uint32_t blocked;
    uint32_t reply_timeouts;
    uint32_t configuration_requests;
} app_xdz_status_t;
/* variables ----------------------------------------------------------------*/

extern app_xdz_status_t g_xdz;
/* function  ----------------------------------------------------------------*/
void    app_xdz_init(void);
void    app_xdz_discard_pending(void);
uint8_t app_xdz_task(void);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* APP_XDZ_H */
/* end of file --------------------------------------------------------------*/
