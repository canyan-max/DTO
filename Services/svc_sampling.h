/**
 ******************************************************************************
 *@file               :   svc_sampling.h
 *@brief              :   Scheduled PA0 sampling, filtered voltage and validity.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef SVC_SAMPLING_H
#define SVC_SAMPLING_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/
typedef struct SVC_SAMPLING_STATUS_T
{
    uint16_t raw;
    uint16_t filtered_raw;
    uint16_t adc_pin_mv;
    uint16_t input_mv;
    uint32_t samples;
    uint32_t updates;
    uint32_t errors;
    uint32_t timeouts;
    uint32_t last_sample_ms;
    uint32_t last_update_ms;
    uint8_t  valid;
    uint8_t  initialized;
    uint8_t  active;
} svc_sampling_status_t;

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
/* All APIs run in main context. Failures invalidate data and retry later. */
void    svc_sampling_init(void);
void    svc_sampling_task(void);
uint8_t svc_sampling_get_status(svc_sampling_status_t *p_status);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_SAMPLING_H */
/* end of file --------------------------------------------------------------*/
