/**
 ******************************************************************************
 *@file               :   svc_params.h
 *@brief              :   Single-page settings using the legacy record format.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef SVC_PARAMS_H
#define SVC_PARAMS_H

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
typedef enum SVC_PARAMS_SOURCE_T
{
    SVC_PARAMS_DEFAULTS = 0U,
    SVC_PARAMS_FLASH,
} svc_params_source_t;
typedef struct SVC_PARAMS_STATUS_T
{
    svc_params_source_t source;
    uint32_t            saves;
    uint32_t            failures;
} svc_params_status_t;

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
/* Startup only. No automatic Flash write or erase. */
void    svc_params_init(void);
uint8_t svc_params_get(comm_params_t *p_params);
uint8_t svc_params_set(const comm_params_t *p_params);
/* Caller must pause business/RX input and drain all TX before writing.
 * Settings take effect on UARTs at next boot, not immediately. */
uint8_t svc_params_save(void);
uint8_t svc_params_save_defaults(void);
uint8_t svc_params_get_status(svc_params_status_t *p_status);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_PARAMS_H */
/* end of file --------------------------------------------------------------*/
