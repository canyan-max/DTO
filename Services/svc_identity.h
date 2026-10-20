/**
 ******************************************************************************
 *@file               :   svc_identity.h
 *@brief              :   Volatile SN cache with no persistent-storage access.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef SVC_IDENTITY_H
#define SVC_IDENTITY_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/
#define SVC_IDENTITY_SN_MAX (16U)

/* typedef ------------------------------------------------------------------*/
typedef struct SVC_IDENTITY_STATUS_T
{
    /* Exact 16 wire bytes, including leading 0x00 padding; byte 16 is a guard.
     */
    char     sn[SVC_IDENTITY_SN_MAX + 1U];
    uint8_t  length;
    uint32_t writes;
} svc_identity_status_t;

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
/* Main context only; initialize once at startup, not when switching protocols.
 */
void    svc_identity_init(void);
uint8_t svc_identity_set_sn(const uint8_t *p_data, uint16_t length);
uint8_t svc_identity_get_status(svc_identity_status_t *p_status);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_IDENTITY_H */
/* end of file --------------------------------------------------------------*/
