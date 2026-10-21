/**
 ******************************************************************************
 *@file               :   comm_params.c
 *@brief              :   Persistent settings shared without HAL types.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "comm_params.h"
#include "comm_config.h"
#include "params_config.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* private  functions  ------------------------------------------------------*/

/* exported functions -------------------------------------------------------*/
uint8_t comm_params_baud_valid(uint32_t baud)
{
    static const uint32_t g_rates[] = {
        1200U, 2400U, 4800U, 9600U, 19200U, 38400U, 57600U, 115200U,
    };
    uint32_t index;
    for(index = 0U; index < sizeof(g_rates) / sizeof(g_rates[0U]); index++)
    {
        if(g_rates[index] == baud)
        {
            return 1U;
        }
    }
    return 0U;
}
uint8_t comm_params_valid(const comm_params_t *p_params)
{
    if(NULL == p_params)
    {
        return 0U;
    }
    return 0U != comm_params_baud_valid(p_params->rs485_1_baud) &&
           0U != comm_params_baud_valid(p_params->rs485_2_baud) &&
           0U != p_params->uv_protect_mv &&
           p_params->uv_recover_mv > p_params->uv_protect_mv &&
           p_params->uv_enabled <= 1U &&
           p_params->app_mode < COMM_APP_MODE_NUM &&
           p_params->relay_contact_mask <= 3U;
}
void comm_params_defaults(comm_params_t *p_params)
{
    if(NULL != p_params)
    {
        p_params->rs485_1_baud       = COMM_UART2_BAUD_RATE;
        p_params->rs485_2_baud       = COMM_UART3_BAUD_RATE;
        p_params->uv_protect_mv      = PARAMS_DEFAULT_UV_PROTECT_MV;
        p_params->uv_recover_mv      = PARAMS_DEFAULT_UV_RECOVER_MV;
        p_params->uv_enabled         = 0U;
        p_params->app_mode           = COMM_APP_MODE_DTO;
        p_params->relay_contact_mask = 3U;
    }
}

/* end of file --------------------------------------------------------------*/
