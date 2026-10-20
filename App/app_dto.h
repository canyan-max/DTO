/**
 ******************************************************************************
 *@file               :   app_dto.h
 *@brief              :   DTO broadcast and queued upstream replies.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef APP_DTO_H
#define APP_DTO_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/
typedef struct APP_DTO_STATS_T
{
    uint32_t downlink_queued;
    uint32_t uplink_queued[2U];
    uint32_t blocked_attempts[3U];
    uint32_t errors;
    uint32_t config_requests;
    uint32_t config_invalid;
    uint32_t config_pending_dropped;
    uint32_t local_pending_dropped;
} app_dto_stats_t;

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
/* Main context only; startup state is zero-initialized. */
void    app_dto_discard_pending(void);
uint8_t app_dto_task(void);
uint8_t app_dto_get_stats(app_dto_stats_t *p_stats);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* APP_DTO_H */
/* end of file --------------------------------------------------------------*/
