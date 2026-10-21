/**
 ******************************************************************************
 *@file               :   bsp_uart.c
 *@brief              :   Three UART interrupt drivers with deferred RX recovery.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "usart.h"
#include "bsp_uart.h"
#include "bsp_system.h"
#include "comm_params.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/
typedef struct BSP_UART_DEVICE_T
{
    uint8_t                      rx_byte;
    volatile uint8_t             rx_fault;
    volatile bsp_uart_tx_state_t tx_state;
    uint32_t                     recover_ms;
    uint32_t                     tx_start_ms;
    uint32_t                     tx_timeout_ms;
} bsp_uart_device_t;

/* variables ----------------------------------------------------------------*/
static UART_HandleTypeDef *const p_handles[] = {&huart1, &huart2, &huart3};
static bsp_uart_device_t         g_devices[COMM_PORT_NUM];
static bsp_uart_event_callback_t pf_event;
static uint8_t                   g_initialized;
static volatile uint8_t          g_rx_suspended;

/* private  functions  ------------------------------------------------------*/
static comm_port_t find_port(const UART_HandleTypeDef *p_handle)
{
    uint32_t index;
    for(index = 0U; index < COMM_PORT_NUM; index++)
    {
        if(p_handles[index] == p_handle)
        {
            return (comm_port_t)index;
        }
    }
    return COMM_PORT_NUM;
}
static void rx_fault(comm_port_t port)
{
    UART_HandleTypeDef *p_handle = p_handles[port];
    bsp_uart_device_t  *p_device = &g_devices[port];
    if(0U == p_device->rx_fault)
    {
        p_device->rx_fault   = 1U;
        p_device->recover_ms = HAL_GetTick() - COMM_UART_RECOVERY_MS;
        pf_event(port, BSP_UART_EVENT_RX_ERROR, 0U, HAL_GetTick());
    }
    /* Avoid repeated error IRQs; recovery runs in the main loop. */
    __HAL_UART_DISABLE_IT(p_handle, UART_IT_RXNE);
    __HAL_UART_DISABLE_IT(p_handle, UART_IT_PE);
    __HAL_UART_DISABLE_IT(p_handle, UART_IT_ERR);
}

/* exported functions -------------------------------------------------------*/
uint8_t bsp_uart_startup_baud(uint32_t rs485_1_baud, uint32_t rs485_2_baud)
{
    if(0U != g_initialized || 0U == comm_params_baud_valid(rs485_1_baud) ||
       0U == comm_params_baud_valid(rs485_2_baud))
    {
        return 0U;
    }
    huart2.Init.BaudRate = rs485_1_baud;
    huart3.Init.BaudRate = rs485_2_baud;
    if(HAL_OK != HAL_RS485Ex_Init(&huart2, UART_DE_POLARITY_HIGH, 16U, 16U) ||
       HAL_OK != HAL_RS485Ex_Init(&huart3, UART_DE_POLARITY_HIGH, 16U, 16U))
    {
        return 0U;
    }
    return 1U;
}
uint8_t bsp_uart_init(bsp_uart_event_callback_t pf_callback)
{
    uint32_t index;
    if(NULL == pf_callback || 0U != g_initialized)
    {
        return 0U;
    }
    for(index = 0U; index < COMM_PORT_NUM; index++)
    {
        if(NULL != p_handles[index]->hdmarx ||
           NULL != p_handles[index]->hdmatx ||
           0U == p_handles[index]->Init.BaudRate)
        {
            return 0U;
        }
    }
    pf_event = pf_callback;
    for(index = 0U; index < COMM_PORT_NUM; index++)
    {
        if(HAL_OK !=
           HAL_UART_Receive_IT(p_handles[index], &g_devices[index].rx_byte, 1U))
        {
            /* Caller treats partial startup failure as fatal. */
            rx_fault((comm_port_t)index);
            return 0U;
        }
    }
    g_initialized = 1U;
    return 1U;
}
void bsp_uart_task(void)
{
    uint32_t index;
    if(0U == g_initialized || 0U != g_rx_suspended)
    {
        return;
    }
    for(index = 0U; index < COMM_PORT_NUM; index++)
    {
        bsp_uart_device_t  *p_device  = &g_devices[index];
        UART_HandleTypeDef *p_handle  = p_handles[index];
        uint32_t            now_ms    = HAL_GetTick();
        uint32_t            irq_state = bsp_system_irq_save();
        if(0U != p_device->rx_fault &&
           (uint32_t)(now_ms - p_device->recover_ms) >= COMM_UART_RECOVERY_MS)
        {
            p_device->recover_ms = now_ms;
            /* No DMA: HAL abort only updates registers and handle state. */
            if(HAL_OK == HAL_UART_AbortReceive(p_handle))
            {
                if(HAL_OK ==
                   HAL_UART_Receive_IT(p_handle, &p_device->rx_byte, 1U))
                {
                    p_device->rx_fault = 0U;
                    /* Discard the damaged frame through the next quiet gap. */
                    pf_event((comm_port_t)index, BSP_UART_EVENT_RX_RECOVERED,
                             0U, now_ms);
                }
            }
        }
        if(BSP_UART_TX_BUSY == p_device->tx_state &&
           (uint32_t)(now_ms - p_device->tx_start_ms) >=
               p_device->tx_timeout_ms)
        {
            if(HAL_OK == HAL_UART_AbortTransmit(p_handle))
            {
                p_device->tx_state = BSP_UART_TX_FAILED;
            }
            /* On abort failure retain the buffer and retry next task. */
        }
        bsp_system_irq_restore(irq_state);
    }
}
bsp_uart_result_t
bsp_uart_send(comm_port_t port, const uint8_t *p_data, uint16_t length)
{
    HAL_StatusTypeDef  result;
    uint32_t           irq_state;
    bsp_uart_device_t *p_device;
    if((uint32_t)port >= COMM_PORT_NUM || NULL == p_data || 0U == length ||
       length > COMM_SERIAL_FRAME_SIZE || 0U == g_initialized)
    {
        return BSP_UART_ERROR;
    }
    p_device  = &g_devices[port];
    irq_state = bsp_system_irq_save();
    if(BSP_UART_TX_BUSY == p_device->tx_state)
    {
        bsp_system_irq_restore(irq_state);
        return BSP_UART_BUSY;
    }
    p_device->tx_start_ms = HAL_GetTick();
    /* Existing CubeMX UART configuration is 8N1: ten bits per byte. */
    p_device->tx_timeout_ms = ((uint32_t)length * 10000U +
                               p_handles[port]->Init.BaudRate - 1U) /
                                  p_handles[port]->Init.BaudRate +
                              COMM_UART_TX_MARGIN_MS;
    p_device->tx_state      = BSP_UART_TX_BUSY;
    result = HAL_UART_Transmit_IT(p_handles[port], (uint8_t *)p_data, length);
    if(HAL_OK != result)
    {
        p_device->tx_state = BSP_UART_TX_IDLE;
    }
    bsp_system_irq_restore(irq_state);
    if(HAL_BUSY == result)
    {
        return BSP_UART_BUSY;
    }
    return HAL_OK == result ? BSP_UART_OK : BSP_UART_ERROR;
}
bsp_uart_tx_state_t bsp_uart_tx_state(comm_port_t port)
{
    if((uint32_t)port >= COMM_PORT_NUM)
    {
        return BSP_UART_TX_FAILED;
    }
    return g_devices[port].tx_state;
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *p_handle)
{
    comm_port_t port = find_port(p_handle);
    if(COMM_PORT_NUM == port || NULL == pf_event || 0U != g_rx_suspended)
    {
        return;
    }
    if(HAL_UART_ERROR_NONE != p_handle->ErrorCode)
    {
        rx_fault(port);
        return;
    }
    if(0U == g_devices[port].rx_fault)
    {
        pf_event(port, BSP_UART_EVENT_RX_BYTE, g_devices[port].rx_byte,
                 HAL_GetTick());
        if(HAL_OK !=
           HAL_UART_Receive_IT(p_handle, &g_devices[port].rx_byte, 1U))
        {
            rx_fault(port);
        }
    }
}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *p_handle)
{
    comm_port_t port = find_port(p_handle);
    if(COMM_PORT_NUM != port && BSP_UART_TX_BUSY == g_devices[port].tx_state)
    {
        /* HAL invokes this only after TC: the final stop bit has left. */
        g_devices[port].tx_state = BSP_UART_TX_COMPLETE;
    }
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *p_handle)
{
    comm_port_t port = find_port(p_handle);
    if(COMM_PORT_NUM != port && NULL != pf_event && 0U == g_rx_suspended)
    {
        rx_fault(port);
    }
}

uint8_t bsp_uart_suspend_rx(uint8_t suspended)
{
    uint32_t index;
    uint32_t irq_state;
    uint8_t  success = 1U;
    if(0U == g_initialized || suspended > 1U)
    {
        return 0U;
    }
    if(suspended == g_rx_suspended)
    {
        return 1U;
    }
    irq_state = bsp_system_irq_save();
    for(index = 0U; index < COMM_PORT_NUM; index++)
    {
        if(BSP_UART_TX_BUSY == g_devices[index].tx_state)
        {
            bsp_system_irq_restore(irq_state);
            return 0U;
        }
    }
    g_rx_suspended = 1U;
    for(index = 0U; index < COMM_PORT_NUM; index++)
    {
        if(0U != suspended)
        {
            if(HAL_OK != HAL_UART_AbortReceive(p_handles[index]))
            {
                success = 0U;
            }
        }
        else
        {
            g_devices[index].rx_fault = 0U;
            if(HAL_OK != HAL_UART_Receive_IT(p_handles[index],
                                             &g_devices[index].rx_byte, 1U))
            {
                success = 0U;
            }
        }
    }
    if(0U == suspended && 0U != success)
    {
        g_rx_suspended = 0U;
    }
    bsp_system_irq_restore(irq_state);
    return success;
}
/* end of file --------------------------------------------------------------*/
