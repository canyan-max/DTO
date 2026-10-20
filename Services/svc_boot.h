/**
 ******************************************************************************
 *@file               :   svc_boot.h
 *@brief              :   Legacy application marker and explicit upgrade flag requests.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef SVC_BOOT_H
#define SVC_BOOT_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/
typedef struct SVC_BOOT_STATUS_T
{
    uint32_t startup_marks;
    uint32_t requests;
    uint32_t failures;
    uint32_t layout_rejects;
} svc_boot_status_t;

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
/* Main context only. Direct-address builds skip startup Flash writes. */
void    svc_boot_init(void);
uint8_t svc_boot_ready(void);
/* Caller has suspended RX and drained all TX; this function never resets. */
uint8_t svc_boot_request(void);
uint8_t svc_boot_get_status(svc_boot_status_t *p_status);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_BOOT_H */
/* end of file --------------------------------------------------------------*/
