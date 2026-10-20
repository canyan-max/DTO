/**
 ******************************************************************************
 *@file               :   comm_params.h
 *@brief              :   Persistent settings shared without HAL types.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef COMM_PARAMS_H
#define COMM_PARAMS_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/
typedef enum COMM_APP_MODE_T
{
    COMM_APP_MODE_DTO = 0U,
    COMM_APP_MODE_HIK,
    COMM_APP_MODE_NUM,
} comm_app_mode_t;

typedef struct COMM_PARAMS_T
{
    uint32_t rs485_1_baud;
    uint32_t rs485_2_baud;
    uint16_t uv_protect_mv;
    uint16_t uv_recover_mv;
    uint8_t  uv_enabled;
    uint8_t  app_mode;
    uint8_t  relay_contact_mask; /* bit n: relay n NO=1, NC=0 */
} comm_params_t;

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
uint8_t comm_params_baud_valid(uint32_t baud);
uint8_t comm_params_valid(const comm_params_t *p_params);
void    comm_params_defaults(comm_params_t *p_params);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* COMM_PARAMS_H */
/* end of file --------------------------------------------------------------*/
