/**
 ******************************************************************************
 *@file               :   sampling_config.h
 *@brief              :   ADC schedule, filter and confirmed Program scaling.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef SAMPLING_CONFIG_H
#define SAMPLING_CONFIG_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/
#define SAMPLING_PERIOD_MS (10U)
#define SAMPLING_TIMEOUT_MS (5U)
#define SAMPLING_RETRY_MS (1000U)
#define SAMPLING_STALE_MS (1000U)
#define SAMPLING_FILTER_COUNT (16U)
#define SAMPLING_ADC_FULL_SCALE (4095U)
/* Fixed nominal VDDA; no VREFINT measurement or +200 mV compensation. */
#define SAMPLING_VREF_MV (3300U)
#define SAMPLING_DIVIDER_NUMERATOR (107500U)
#define SAMPLING_DIVIDER_DENOMINATOR (7500U)

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SAMPLING_CONFIG_H */
/* end of file --------------------------------------------------------------*/
