/**
 ******************************************************************************
 *@file               :   svc_identity.c
 *@brief              :   Bounded RAM-only serial-number storage.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include <string.h>
#include "svc_identity.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/
svc_identity_status_t g_sn_cache;

/* private  functions  ------------------------------------------------------*/

/* exported functions -------------------------------------------------------*/
void svc_identity_init(void)
{
    memset(&g_sn_cache, 0, sizeof(g_sn_cache));
}
uint8_t svc_identity_set_sn(const uint8_t *p_data, uint16_t length)
{
    char next[SVC_IDENTITY_SN_MAX + 1U] = {0};
    if(NULL == p_data || SVC_IDENTITY_SN_MAX != length)
    {
        return 0U;
    }
    memcpy(next, p_data, length);
    memcpy(g_sn_cache.sn, next, sizeof(next));
    g_sn_cache.length = (uint8_t)length;
    g_sn_cache.writes++;
    return 1U;
}
uint8_t svc_identity_get_status(svc_identity_status_t *p_status)
{
    if(NULL == p_status)
    {
        return 0U;
    }
    *p_status = g_sn_cache;
    return 1U;
}

/* end of file --------------------------------------------------------------*/
