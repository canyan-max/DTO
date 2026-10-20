/**
 ******************************************************************************
 *@file               :   poll_config.h
 *@brief              :   Controller polling timing and bench enable.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef POLL_CONFIG_H
#define POLL_CONFIG_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/
/* Old scheduler sent at most one query per second and changed profile after
 * five seconds without a valid reply. Response/quiet limits are bench values.
 */
#ifndef COMM_APP_ENABLE_POLL
#define COMM_APP_ENABLE_POLL (1U)
#endif
#define POLL_INTERVAL_MS (1000U)
#define POLL_RESPONSE_MS (600U)
#define POLL_QUEUE_MS (2000U)
#define POLL_RETRY_QUIET_MS (600U)
#define POLL_DETECT_MS (5000U)

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* POLL_CONFIG_H */
/* end of file --------------------------------------------------------------*/
