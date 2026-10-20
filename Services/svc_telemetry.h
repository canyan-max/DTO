/**
 ******************************************************************************
 *@file               :   svc_telemetry.h
 *@brief              :   Independent, expiring battery and solar snapshots.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef SVC_TELEMETRY_H
#define SVC_TELEMETRY_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "telemetry_types.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/
typedef struct SVC_TELEMETRY_SNAPSHOT_T
{
    comm_battery_data_t battery;
    comm_solar_data_t   solar;
    uint32_t            battery_updated_ms;
    uint32_t            solar_updated_ms;
    uint8_t             battery_valid;
    uint8_t             solar_valid;
} svc_telemetry_snapshot_t;

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
/* Main context only. Actual validated device replies publish through these
 * APIs. Local ADC is a separate measurement and is not used as external battery
 * data. */
void    svc_telemetry_init(void);
uint8_t svc_telemetry_publish_battery(const comm_battery_data_t *p_data);
uint8_t svc_telemetry_publish_solar(const comm_solar_data_t *p_data);
uint8_t svc_telemetry_get(svc_telemetry_snapshot_t *p_snapshot);
void    svc_telemetry_invalidate(void);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_TELEMETRY_H */
/* end of file --------------------------------------------------------------*/
