/**
 ******************************************************************************
 *@file               :   app_main.h
 *@brief              :   Stage-one application assembly without protocol routing.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef APP_MAIN_H
#define APP_MAIN_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
/* After system clock setup, before MX_GPIO_Init; read/preload only, no Flash
 * write. */
uint8_t app_prepare_outputs(void);
uint8_t app_init(void);
uint8_t app_task(void);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* APP_MAIN_H */
/* end of file --------------------------------------------------------------*/
