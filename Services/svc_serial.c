/**
 ******************************************************************************
 *@file               :   svc_serial.c
 *@brief              :   Bounded per-port RX frames and queued nonblocking TX.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include <string.h>
#include "svc_serial.h"
#include "bsp_system.h"
#include "bsp_uart.h"
/* define   -----------------------------------------------------------------*/
/* One extra slot is reserved for the frame currently being received. */
#define SERIAL_RX_SLOTS (COMM_SERIAL_QUEUE_DEPTH + 1U)

/* typedef ------------------------------------------------------------------*/
typedef struct SERIAL_FRAME_T
{
    uint16_t length;
    uint8_t  data[COMM_SERIAL_FRAME_SIZE];
} serial_frame_t;
typedef struct SERIAL_PORT_T
{
    serial_frame_t              rx[SERIAL_RX_SLOTS];
    serial_frame_t              tx[COMM_SERIAL_QUEUE_DEPTH];
    volatile uint8_t            rx_head;
    volatile uint8_t            rx_tail;
    volatile uint16_t           rx_length;
    volatile uint8_t            rx_active;
    volatile uint8_t            rx_discard;
    volatile uint32_t           rx_last_ms;
    uint8_t                     tx_head;
    uint8_t                     tx_tail;
    uint8_t                     tx_count;
    uint8_t                     tx_active;
    uint8_t                     tx_waiting;
    uint32_t                    tx_wait_ms;
    volatile svc_serial_stats_t stats;
} serial_port_t;

/* variables ----------------------------------------------------------------*/
serial_port_t         g_ports[COMM_PORT_NUM];
static const uint32_t g_idle_ms[] = {
    COMM_UART1_RX_IDLE_MS,
    COMM_UART2_RX_IDLE_MS,
    COMM_UART3_RX_IDLE_MS,
};
static uint8_t g_initialized;

/* private  functions  ------------------------------------------------------*/
/* Called from UART IRQ or with interrupts guarded; never copies a frame. */
static void finish_rx(serial_port_t *p_port)
{
    uint8_t next = (uint8_t)((p_port->rx_head + 1U) % SERIAL_RX_SLOTS);
    if(0U == p_port->rx_discard && 0U != p_port->rx_length)
    {
        if(next == p_port->rx_tail)
        {
            p_port->stats.rx_queue_full++;
        }
        else
        {
            p_port->rx[p_port->rx_head].length = p_port->rx_length;
            p_port->rx_head                    = next;
            p_port->stats.rx_frames++;
        }
    }
    p_port->rx_active  = 0U;
    p_port->rx_length  = 0U;
    p_port->rx_discard = 0U;
}
static void on_uart_event(comm_port_t      port,
                          bsp_uart_event_t event,
                          uint8_t          value,
                          uint32_t         now_ms)
{
    serial_port_t *p_port = &g_ports[port];
    if(BSP_UART_EVENT_RX_BYTE != event)
    {
        p_port->rx_discard = 1U;
        p_port->rx_active  = 1U;
        p_port->rx_length  = 0U;
        p_port->rx_last_ms = now_ms;
        if(BSP_UART_EVENT_RX_ERROR == event)
        {
            p_port->stats.rx_errors++;
        }
        return;
    }
    if(0U != p_port->rx_active &&
       (uint32_t)(now_ms - p_port->rx_last_ms) >= g_idle_ms[port])
    {
        finish_rx(p_port);
    }
    p_port->rx_active  = 1U;
    p_port->rx_last_ms = now_ms;
    p_port->stats.rx_bytes++;
    if(0U != p_port->rx_discard)
    {
        return;
    }
    if(p_port->rx_length >= COMM_SERIAL_FRAME_SIZE)
    {
        p_port->stats.rx_oversize++;
        p_port->rx_discard = 1U;
        p_port->rx_length  = 0U;
        return;
    }
    p_port->rx[p_port->rx_head].data[p_port->rx_length] = value;
    p_port->rx_length++;
}
static void finish_tx(serial_port_t *p_port, uint8_t success)
{
    if(0U != success)
    {
        p_port->stats.tx_frames++;
    }
    else
    {
        /* Do not retry a possibly partially delivered request. */
        p_port->stats.tx_errors++;
    }
    p_port->tx_tail = (uint8_t)((p_port->tx_tail + 1U) %
                                COMM_SERIAL_QUEUE_DEPTH);
    p_port->tx_count--;
    p_port->tx_active  = 0U;
    p_port->tx_waiting = 0U;
}
static void process_tx(comm_port_t port)
{
    serial_port_t    *p_port = &g_ports[port];
    bsp_uart_result_t result;
    if(0U == p_port->tx_count)
    {
        return;
    }
    if(0U != p_port->tx_active)
    {
        bsp_uart_tx_state_t state = bsp_uart_tx_state(port);
        if(BSP_UART_TX_COMPLETE == state)
        {
            finish_tx(p_port, 1U);
        }
        else if(BSP_UART_TX_FAILED == state)
        {
            finish_tx(p_port, 0U);
        }
        return;
    }
    if(0U == p_port->tx_waiting)
    {
        p_port->tx_waiting = 1U;
        p_port->tx_wait_ms = bsp_system_time_ms();
    }
    result = bsp_uart_send(port, p_port->tx[p_port->tx_tail].data,
                           p_port->tx[p_port->tx_tail].length);
    if(BSP_UART_OK == result)
    {
        p_port->tx_active = 1U;
    }
    else if(BSP_UART_ERROR == result ||
            (uint32_t)(bsp_system_time_ms() - p_port->tx_wait_ms) >=
                COMM_UART_TX_START_TIMEOUT_MS)
    {
        finish_tx(p_port, 0U);
    }
}

/* exported functions -------------------------------------------------------*/
uint8_t svc_serial_init(void)
{
    if(0U != g_initialized)
    {
        return 0U;
    }
    /* Set before starting IRQ reception; callbacks may run during init. */
    g_initialized = 1U;
    if(0U == bsp_uart_init(on_uart_event))
    {
        g_initialized = 0U;
        return 0U;
    }
    return 1U;
}
void svc_serial_task(void)
{
    uint32_t index;
    if(0U == g_initialized)
    {
        return;
    }
    bsp_uart_task();
    for(index = 0U; index < COMM_PORT_NUM; index++)
    {
        serial_port_t *p_port    = &g_ports[index];
        uint32_t       irq_state = bsp_system_irq_save();
        uint32_t       now_ms    = bsp_system_time_ms();
        if(0U != p_port->rx_active &&
           (uint32_t)(now_ms - p_port->rx_last_ms) >= g_idle_ms[index])
        {
            finish_rx(p_port);
        }
        bsp_system_irq_restore(irq_state);
        process_tx((comm_port_t)index);
    }
}
svc_serial_result_t
svc_serial_send(comm_port_t port, const uint8_t *p_data, uint16_t length)
{
    if((uint32_t)port >= COMM_PORT_NUM)
    {
        return SVC_SERIAL_INVALID;
    }
    return svc_serial_send_mask(1U << (uint32_t)port, p_data, length);
}
svc_serial_result_t
svc_serial_send_mask(uint32_t port_mask, const uint8_t *p_data, uint16_t length)
{
    uint32_t index;
    if(0U == port_mask || 0U != (port_mask & ~((1U << COMM_PORT_NUM) - 1U)) ||
       NULL == p_data || 0U == length || length > COMM_SERIAL_FRAME_SIZE)
    {
        return SVC_SERIAL_INVALID;
    }
    if(0U == g_initialized)
    {
        return SVC_SERIAL_NOT_READY;
    }
    /* TX queues have one owner (main). IRQs never mutate their indices. */
    for(index = 0U; index < COMM_PORT_NUM; index++)
    {
        if(0U != (port_mask & (1U << index)) &&
           g_ports[index].tx_count >= COMM_SERIAL_QUEUE_DEPTH)
        {
            g_ports[index].stats.tx_queue_full++;
            return SVC_SERIAL_FULL;
        }
    }
    for(index = 0U; index < COMM_PORT_NUM; index++)
    {
        if(0U != (port_mask & (1U << index)))
        {
            serial_port_t *p_port = &g_ports[index];
            memcpy(p_port->tx[p_port->tx_head].data, p_data, length);
            p_port->tx[p_port->tx_head].length = length;
            p_port->tx_head = (uint8_t)((p_port->tx_head + 1U) %
                                        COMM_SERIAL_QUEUE_DEPTH);
            p_port->tx_count++;
        }
    }
    return SVC_SERIAL_OK;
}
svc_serial_result_t svc_serial_read(comm_port_t port,
                                    uint8_t    *p_data,
                                    uint16_t    capacity,
                                    uint16_t   *p_length)
{
    serial_port_t  *p_port;
    serial_frame_t *p_frame;
    uint32_t        irq_state;
    if(NULL == p_length)
    {
        return SVC_SERIAL_INVALID;
    }
    *p_length = 0U;
    if((uint32_t)port >= COMM_PORT_NUM || NULL == p_data || 0U == capacity)
    {
        return SVC_SERIAL_INVALID;
    }
    if(0U == g_initialized)
    {
        return SVC_SERIAL_NOT_READY;
    }
    p_port = &g_ports[port];
    if(p_port->rx_tail == p_port->rx_head)
    {
        return SVC_SERIAL_EMPTY;
    }
    /* This published slot stays reserved until rx_tail is advanced. */
    p_frame   = &p_port->rx[p_port->rx_tail];
    *p_length = p_frame->length;
    if(capacity < p_frame->length)
    {
        return SVC_SERIAL_BUFFER_SMALL;
    }
    memcpy(p_data, p_frame->data, p_frame->length);
    irq_state       = bsp_system_irq_save();
    p_port->rx_tail = (uint8_t)((p_port->rx_tail + 1U) % SERIAL_RX_SLOTS);
    bsp_system_irq_restore(irq_state);
    return SVC_SERIAL_OK;
}
uint8_t svc_serial_get_stats(comm_port_t port, svc_serial_stats_t *p_stats)
{
    uint32_t irq_state;
    if((uint32_t)port >= COMM_PORT_NUM || NULL == p_stats)
    {
        return 0U;
    }
    irq_state = bsp_system_irq_save();
    *p_stats  = g_ports[port].stats;
    bsp_system_irq_restore(irq_state);
    return 1U;
}

uint8_t svc_serial_tx_pending(comm_port_t port, uint16_t *p_count)
{
    if((uint32_t)port >= COMM_PORT_NUM || NULL == p_count ||
       0U == g_initialized)
    {
        return 0U;
    }
    *p_count = g_ports[port].tx_count;
    return 1U;
}
uint8_t svc_serial_discard_rx(comm_port_t port)
{
    serial_port_t *p_port;
    uint32_t       irq_state;
    if((uint32_t)port >= COMM_PORT_NUM || 0U == g_initialized)
    {
        return 0U;
    }
    p_port             = &g_ports[port];
    irq_state          = bsp_system_irq_save();
    p_port->rx_tail    = p_port->rx_head;
    p_port->rx_length  = 0U;
    p_port->rx_discard = 1U;
    p_port->rx_active  = 1U;
    p_port->rx_last_ms = bsp_system_time_ms();
    bsp_system_irq_restore(irq_state);
    return 1U;
}
/* end of file --------------------------------------------------------------*/
