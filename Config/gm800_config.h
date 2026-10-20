/**
 ******************************************************************************
 *@file               :   gm800_config.h
 *@brief              :   Confirmed legacy GM800 timing and authentication.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef GM800_CONFIG_H
#define GM800_CONFIG_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/
#define GM800_STEP_INTERVAL_MS (200U)
#define GM800_LOG_TAIL_MS (500U)
#define GM800_RESPONSE_TIMEOUT_MS (2000U)
#define GM800_SAVE_TIMEOUT_MS (5000U)
#define GM800_SESSION_TIMEOUT_MS (90000U)
/* User confirmed reuse of the legacy command profile, including this suffix.
 * No default device/cloud identity is copied; it comes from the 485 request. */
#define GM800_CLOUD_AUTH_SUFFIX ",GqiI2Fs4\r\n"

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* GM800_CONFIG_H */
/* end of file --------------------------------------------------------------*/
