/**
 ******************************************************************************
 *@file               :   svc_serial.h
 *@brief              :   Bounded per-port RX frames and queued nonblocking TX.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef SVC_SERIAL_H
#define SVC_SERIAL_H

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
typedef enum SVC_SERIAL_RESULT_T
{
    SVC_SERIAL_OK = 0U,
    SVC_SERIAL_EMPTY,
    SVC_SERIAL_FULL,
    SVC_SERIAL_INVALID,
    SVC_SERIAL_BUFFER_SMALL,
    SVC_SERIAL_NOT_READY,
} svc_serial_result_t;
typedef struct SVC_SERIAL_STATS_T
{
    uint32_t rx_bytes;
    uint32_t rx_frames;
    uint32_t rx_oversize;
    uint32_t rx_queue_full;
    uint32_t rx_errors;
    uint32_t tx_frames;
    uint32_t tx_errors;
    uint32_t tx_queue_full;
} svc_serial_stats_t;

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
/* Main-context API; init once. No protocol parsing or routing here. */
uint8_t svc_serial_init(void);
void    svc_serial_task(void);
/* Copies data: caller may reuse the buffer after return. */
svc_serial_result_t
svc_serial_send(comm_port_t port, const uint8_t *p_data, uint16_t length);
/* Bit n selects comm_port_t n. Main only: enqueue all ports or none. */
svc_serial_result_t svc_serial_send_mask(uint32_t       port_mask,
                                         const uint8_t *p_data,
                                         uint16_t       length);
/* On BUFFER_SMALL, returns required length without removing the frame. */
svc_serial_result_t svc_serial_read(comm_port_t port,
                                    uint8_t    *p_data,
                                    uint16_t    capacity,
                                    uint16_t   *p_length);
uint8_t svc_serial_get_stats(comm_port_t port, svc_serial_stats_t *p_stats);
/* Main only. Count includes the currently transmitting frame. */
uint8_t svc_serial_tx_pending(comm_port_t port, uint16_t *p_count);
/* Configuration boundary: discard RX queues and wait for a fresh quiet gap. */
uint8_t svc_serial_discard_rx(comm_port_t port);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_SERIAL_H */
/* end of file --------------------------------------------------------------*/
