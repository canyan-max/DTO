/**
 ******************************************************************************
 *@file               :   svc_params.c
 *@brief              :   Single-page settings using the legacy record format.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include <string.h>
#include "svc_params.h"
#include "bsp_nv.h"
#include "params_config.h"
#include "svc_gm800.h"
#include "svc_serial.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/
comm_params_t       g_params;
svc_params_status_t g_status;
uint8_t             g_initialized;
static char         g_cloud_identity[PARAMS_CLOUD_ID_SIZE + 1U];

/* private  functions  ------------------------------------------------------*/
static uint32_t checksum(const uint64_t *p_words)
{
    return (uint32_t)p_words[0U] ^ (uint32_t)(p_words[0U] >> 32U) ^
           (uint32_t)p_words[1U] ^ (uint32_t)(p_words[1U] >> 32U);
}
static uint8_t decode(const uint64_t *p_words, comm_params_t *p_params)
{
    if((uint32_t)p_words[2U] != PARAMS_VALID_MAGIC ||
       (uint32_t)(p_words[2U] >> 32U) != checksum(p_words) ||
       0U != (p_words[1U] >> 56U))
    {
        return 0U;
    }
    p_params->rs485_1_baud  = (uint32_t)p_words[0U];
    p_params->rs485_2_baud  = (uint32_t)(p_words[0U] >> 32U);
    p_params->uv_protect_mv = (uint16_t)p_words[1U];
    p_params->uv_recover_mv = (uint16_t)(p_words[1U] >> 16U);
    p_params->uv_enabled    = (uint8_t)(p_words[1U] >> 32U);
    /* Legacy records kept this byte zero, which is the default DTO mode. */
    p_params->app_mode           = (uint8_t)(p_words[1U] >> 40U);
    p_params->relay_contact_mask = (uint8_t)(p_words[1U] >> 48U);
    return comm_params_valid(p_params);
}
static uint32_t cloud_checksum(const uint64_t *p_words)
{
    uint32_t value = 2166136261U;
    uint16_t index;
    for(index = 0U; index < 24U; index++)
    {
        value ^= (uint8_t)(p_words[3U + index / 8U] >> ((index % 8U) * 8U));
        value *= 16777619U;
    }
    return value;
}
static void decode_cloud_id(const uint64_t *p_words)
{
    char     identity[PARAMS_CLOUD_ID_SIZE + 1U];
    uint16_t index;
    if((uint32_t)p_words[6U] != PARAMS_CLOUD_ID_MAGIC ||
       (uint32_t)(p_words[6U] >> 32U) != cloud_checksum(p_words) ||
       0U != (p_words[5U] >> 32U))
    {
        return;
    }
    for(index = 0U; index < PARAMS_CLOUD_ID_SIZE; index++)
    {
        identity[index] = (char)(p_words[3U + index / 8U] >>
                                 ((index % 8U) * 8U));
        if(identity[index] < '0' || identity[index] > '9')
        {
            return;
        }
    }
    identity[PARAMS_CLOUD_ID_SIZE] = '\0';
    memcpy(g_cloud_identity, identity, sizeof(identity));
}
static uint8_t commit(const comm_params_t *p_params, const char *p_identity)
{
    uint64_t words[PARAMS_RECORD_WORDS];
    uint64_t verify[PARAMS_RECORD_WORDS];
    uint32_t index;
    uint16_t pending;
    if(0U != svc_gm800_busy())
    {
        return 0U;
    }
    for(index = 0U; index < COMM_PORT_NUM; index++)
    {
        if(0U == svc_serial_tx_pending((comm_port_t)index, &pending) ||
           0U != pending)
        {
            return 0U;
        }
    }
    words[0U] = (uint64_t)p_params->rs485_1_baud |
                ((uint64_t)p_params->rs485_2_baud << 32U);
    words[1U] = (uint64_t)p_params->uv_protect_mv |
                ((uint64_t)p_params->uv_recover_mv << 16U) |
                ((uint64_t)p_params->uv_enabled << 32U) |
                ((uint64_t)p_params->app_mode << 40U) |
                ((uint64_t)p_params->relay_contact_mask << 48U);
    words[2U] = (uint64_t)PARAMS_VALID_MAGIC |
                ((uint64_t)checksum(words) << 32U);
    words[3U] = 0U;
    words[4U] = 0U;
    words[5U] = 0U;
    if('\0' != p_identity[0U])
    {
        for(index = 0U; index < PARAMS_CLOUD_ID_SIZE; index++)
        {
            words[3U + index / 8U] |= (uint64_t)(uint8_t)p_identity[index]
                                      << ((index % 8U) * 8U);
        }
    }
    words[6U] = (uint64_t)PARAMS_CLOUD_ID_MAGIC |
                ((uint64_t)cloud_checksum(words) << 32U);
    /* Identical settings need no erase, even for repeated save requests. */
    if(0U != bsp_nv_read(verify, PARAMS_RECORD_WORDS))
    {
        for(index = 0U; index < PARAMS_RECORD_WORDS; index++)
        {
            if(words[index] != verify[index])
            {
                break;
            }
        }
        if(PARAMS_RECORD_WORDS == index)
        {
            g_status.source = SVC_PARAMS_FLASH;
            return 1U;
        }
    }
    if(0U == bsp_nv_write(words, PARAMS_RECORD_WORDS) ||
       0U == bsp_nv_read(verify, PARAMS_RECORD_WORDS))
    {
        g_status.failures++;
        return 0U;
    }
    for(index = 0U; index < PARAMS_RECORD_WORDS; index++)
    {
        if(words[index] != verify[index])
        {
            g_status.failures++;
            return 0U;
        }
    }
    g_status.source = SVC_PARAMS_FLASH;
    g_status.saves++;
    return 1U;
}

/* exported functions -------------------------------------------------------*/
void svc_params_init(void)
{
    uint64_t words[PARAMS_RECORD_WORDS];
    comm_params_defaults(&g_params);
    memset(g_cloud_identity, 0, sizeof(g_cloud_identity));
    g_status.source   = SVC_PARAMS_DEFAULTS;
    g_status.saves    = 0U;
    g_status.failures = 0U;
    if(0U != bsp_nv_read(words, PARAMS_RECORD_WORDS))
    {
        comm_params_t saved;
        if(0U != decode(words, &saved))
        {
            g_params        = saved;
            g_status.source = SVC_PARAMS_FLASH;
            decode_cloud_id(words);
        }
    }
    g_initialized = 1U;
}
uint8_t svc_params_get(comm_params_t *p_params)
{
    if(NULL == p_params || 0U == g_initialized)
    {
        return 0U;
    }
    *p_params = g_params;
    return 1U;
}
uint8_t svc_params_set(const comm_params_t *p_params)
{
    if(0U == g_initialized || 0U == comm_params_valid(p_params))
    {
        return 0U;
    }
    g_params = *p_params;
    return 1U;
}
uint8_t svc_params_save(void)
{
    if(0U == g_initialized)
    {
        return 0U;
    }
    return commit(&g_params, g_cloud_identity);
}
uint8_t svc_params_get_cloud_id(char *p_identity, uint16_t capacity)
{
    if(NULL == p_identity || capacity < sizeof(g_cloud_identity) ||
       0U == g_initialized)
    {
        return 0U;
    }
    memcpy(p_identity, g_cloud_identity, sizeof(g_cloud_identity));
    return 1U;
}
uint8_t svc_params_set_cloud_id(const char *p_identity, uint16_t capacity)
{
    uint16_t index;
    if(NULL == p_identity || capacity < sizeof(g_cloud_identity) ||
       0U == g_initialized)
    {
        return 0U;
    }
    for(index = 0U; index < PARAMS_CLOUD_ID_SIZE; index++)
    {
        if(p_identity[index] < '0' || p_identity[index] > '9')
        {
            return 0U;
        }
    }
    if('\0' != p_identity[PARAMS_CLOUD_ID_SIZE])
    {
        return 0U;
    }
    memcpy(g_cloud_identity, p_identity, sizeof(g_cloud_identity));
    return 1U;
}
uint8_t svc_params_save_defaults(void)
{
    comm_params_t defaults;
    if(0U == g_initialized)
    {
        return 0U;
    }
    comm_params_defaults(&defaults);
    if(0U == commit(&defaults, ""))
    {
        return 0U;
    }
    g_params = defaults;
    memset(g_cloud_identity, 0, sizeof(g_cloud_identity));
    return 1U;
}
uint8_t svc_params_get_status(svc_params_status_t *p_status)
{
    if(NULL == p_status)
    {
        return 0U;
    }
    *p_status = g_status;
    return 1U;
}

/* end of file --------------------------------------------------------------*/
