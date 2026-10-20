/**
 ******************************************************************************
 *@file               :   bsp_uart.h
 *@brief              :   Three UART interrupt drivers with deferred RX recovery.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef BSP_UART_H
#define BSP_UART_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "comm_config.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/
typedef enum BSP_UART_EVENT_T
{
    BSP_UART_EVENT_RX_BYTE = 0U,
    BSP_UART_EVENT_RX_ERROR,
    BSP_UART_EVENT_RX_RECOVERED,
} bsp_uart_event_t;
typedef enum BSP_UART_TX_STATE_T
{
    BSP_UART_TX_IDLE = 0U,
    BSP_UART_TX_BUSY,
    BSP_UART_TX_COMPLETE,
    BSP_UART_TX_FAILED,
} bsp_uart_tx_state_t;
typedef enum BSP_UART_RESULT_T
{
    BSP_UART_OK = 0U,
    BSP_UART_BUSY,
    BSP_UART_ERROR,
} bsp_uart_result_t;
/* Callback runs in IRQ or a short IRQ guard; no blocking or HAL calls. */
typedef void (*bsp_uart_event_callback_t)(comm_port_t      port,
                                          bsp_uart_event_t event,
                                          uint8_t          value,
                                          uint32_t         now_ms);

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
/* Call once, after CubeMX initialization. DMA is unsupported here. */
uint8_t bsp_uart_init(bsp_uart_event_callback_t pf_callback);
/* Before bsp_uart_init only; retains hardware RS485 DE and 8N1. */
uint8_t bsp_uart_startup_baud(uint32_t rs485_1_baud, uint32_t rs485_2_baud);
void    bsp_uart_task(void);
/* TX buffer must remain valid until COMPLETE or FAILED. Main context only. */
bsp_uart_result_t
bsp_uart_send(comm_port_t port, const uint8_t *p_data, uint16_t length);
bsp_uart_tx_state_t bsp_uart_tx_state(comm_port_t port);
/* Main only. All TX must be drained before suspending RX for Flash writes. */
uint8_t bsp_uart_suspend_rx(uint8_t suspended);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* BSP_UART_H */
/* end of file --------------------------------------------------------------*/
