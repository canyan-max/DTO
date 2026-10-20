/**
 ******************************************************************************
 *@file               :   comm_config.h
 *@brief              :   Port roles and provisional serial resource limits.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef COMM_CONFIG_H
#define COMM_CONFIG_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/
/* Match CubeMX; these do not overwrite generated UART initialization. */
#define COMM_UART1_BAUD_RATE (115200U)
#define COMM_UART2_BAUD_RATE (9600U)
#define COMM_UART3_BAUD_RATE (9600U)
/* Production enables both implementations, with one runtime RX owner.
 * Single-mode compile switches remain available for isolated bench tests. */
#ifndef COMM_APP_ENABLE_MODE
#define COMM_APP_ENABLE_MODE (1U)
#endif
#ifndef COMM_APP_ENABLE_DTO
#define COMM_APP_ENABLE_DTO (COMM_APP_ENABLE_MODE)
#endif
#ifndef COMM_APP_ENABLE_HIK
#define COMM_APP_ENABLE_HIK (COMM_APP_ENABLE_MODE || !COMM_APP_ENABLE_DTO)
#endif
#if COMM_APP_ENABLE_MODE && (!COMM_APP_ENABLE_DTO || !COMM_APP_ENABLE_HIK)
#error "Runtime mode switching requires both implementations"
#endif
#if !COMM_APP_ENABLE_MODE && COMM_APP_ENABLE_DTO && COMM_APP_ENABLE_HIK
#error "Single-mode bench build cannot have two RX owners"
#endif
#define COMM_MODE_SWITCH_TIMEOUT_MS (20000U)
/* Cover the outstanding controller response window and RX framing silence. */
#define COMM_MODE_SWITCH_QUIET_MS (700U)
/* Initial migration values, to be validated with real traffic. */
#define COMM_SERIAL_FRAME_SIZE (256U)
#define COMM_SERIAL_QUEUE_DEPTH (4U)
#define COMM_UART1_RX_IDLE_MS (15U)
#define COMM_UART2_RX_IDLE_MS (50U)
#define COMM_UART3_RX_IDLE_MS (50U)
#define COMM_UART_RECOVERY_MS (10U)
#define COMM_UART_TX_MARGIN_MS (100U)
#define COMM_UART_TX_START_TIMEOUT_MS (1000U)

/* typedef ------------------------------------------------------------------*/
typedef enum COMM_PORT_T
{
    COMM_PORT_4G = 0U,
    COMM_PORT_RS485_1,
    COMM_PORT_RS485_2,
    COMM_PORT_NUM,
} comm_port_t;

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* COMM_CONFIG_H */
/* end of file --------------------------------------------------------------*/
