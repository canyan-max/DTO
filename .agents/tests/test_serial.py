"""Host regression tests for actual serial service and UART BSP, with fake HAL.

Run: python -X utf8 .agents/tests/test_serial.py
Requires a native GCC (not arm-none-eabi-gcc) in PATH. No board or flash access.
"""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]

HAL_HEADER = r"""
#ifndef TEST_USART_H
#define TEST_USART_H
#include <stdint.h>
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY } HAL_StatusTypeDef;
typedef struct {
    struct { uint32_t BaudRate; } Init;
    void *hdmarx;
    void *hdmatx;
    uint32_t ErrorCode;
} UART_HandleTypeDef;
extern UART_HandleTypeDef huart1, huart2, huart3;
#define HAL_UART_ERROR_NONE (0U)
#define UART_IT_RXNE (1U)
#define UART_IT_PE (2U)
#define UART_IT_ERR (3U)
#define UART_DE_POLARITY_HIGH (1U)
#define __HAL_UART_DISABLE_IT(h, i) test_disable(h, i)
void test_disable(UART_HandleTypeDef *p_handle, uint32_t interrupt);
uint32_t HAL_GetTick(void);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *, uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *, uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *);
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *);
HAL_StatusTypeDef HAL_RS485Ex_Init(UART_HandleTypeDef *, uint32_t,
                                   uint32_t, uint32_t);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *);
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *);
void HAL_UART_ErrorCallback(UART_HandleTypeDef *);
#endif
"""

HARNESS = r"""
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "usart.h"
#include "svc_serial.h"
#include "bsp_uart.h"
#include "bsp_system.h"
#include "bsp_boot.h"
#include "boot_config.h"
/* Communication regressions isolate the independently tested ADC service. */
#include "svc_sampling.h"
static svc_sampling_status_t g_test_sampling;
void svc_sampling_init(void) { memset(&g_test_sampling,0,sizeof(g_test_sampling)); }
void svc_sampling_task(void) {}
uint8_t svc_sampling_get_status(svc_sampling_status_t *p_status)
{
    if(NULL==p_status) { return 0U; }
    *p_status=g_test_sampling;
    return 1U;
}

UART_HandleTypeDef huart1 = { {115200U}, NULL, NULL, 0U };
UART_HandleTypeDef huart2 = { {9600U}, NULL, NULL, 0U };
UART_HandleTypeDef huart3 = { {9600U}, NULL, NULL, 0U };
static UART_HandleTypeDef *const p_handles[] = { &huart1, &huart2, &huart3 };
static uint8_t *p_rx[3];
static const uint8_t *p_tx[3];
static uint16_t g_tx_len[3];
static uint32_t g_now;
static uint32_t g_irq;
static uint32_t g_rx_fail;
static uint32_t g_abort_tx_fail;
static uint32_t g_force_busy;
static uint32_t g_abort_count;
static uint32_t g_tx_calls;
static uint8_t g_tx_log[32];
static uint64_t g_nv_data[3];
static uint8_t g_nv_ready;
static uint32_t g_reset_count;
static uint64_t g_relay_nv = UINT64_MAX;
static unsigned g_relay_nv_writes;
static uint8_t g_relay_nv_fail;
static uint8_t g_relay_gpio[2U];
static uint8_t g_led_gpio[3U];
uint8_t bsp_io_set_relay(uint8_t relay,uint8_t level)
{
    assert(relay<2U && level<=1U);
    g_relay_gpio[relay]=level;
    return 1U;
}
uint8_t bsp_io_set_led(uint8_t led,uint8_t level)
{
    assert(led<3U && level<=1U);
    g_led_gpio[led]=level;
    return 1U;
}
uint8_t bsp_nv_read_relays(uint64_t *p_word)
{
    if(NULL==p_word) { return 0U; }
    *p_word=g_relay_nv;
    return 1U;
}
uint8_t bsp_nv_write_relays(uint64_t word)
{
    assert(word<=3U);
    assert(NULL==p_rx[0U] && NULL==p_rx[1U] && NULL==p_rx[2U]);
    g_relay_nv_writes++;
    if(g_relay_nv_fail) { return 0U; }
    g_relay_nv=word;
    return 1U;
}
static uint8_t g_test_boot_layout;
static uint8_t g_test_boot_fail;
static uint8_t g_test_boot_read_fail;
static uint64_t g_test_boot_flag = UINT64_MAX;
static unsigned g_test_boot_writes;
uint8_t bsp_boot_layout_valid(void) { return g_test_boot_layout; }
uint8_t bsp_boot_read_flag(uint64_t *p_word)
{
    if(!p_word || !g_test_boot_layout || g_test_boot_read_fail) return 0U;
    *p_word=g_test_boot_flag;return 1U;
}
uint8_t bsp_boot_write_flag(uint16_t flag)
{
    if(!g_test_boot_layout) return 0U;
    assert(NULL==p_rx[0]&&NULL==p_rx[1]&&NULL==p_rx[2]);
    assert(NULL==p_tx[0]&&NULL==p_tx[1]&&NULL==p_tx[2]);
    g_test_boot_writes++;
    if(g_test_boot_fail)return 0U;
    g_test_boot_flag=flag;return 1U;
}
void bsp_system_reset(void) { g_reset_count++; }

void test_nv_reset(void)
{
    memset(g_nv_data, 0xFF, sizeof(g_nv_data));
    g_nv_ready = 1U;
}
uint8_t bsp_nv_read(uint64_t *p_words, uint16_t count)
{
    if(0U == g_nv_ready) { test_nv_reset(); }
    if(NULL == p_words || 3U != count) { return 0U; }
    memcpy(p_words, g_nv_data, 24U);
    return 1U;
}
uint8_t bsp_nv_write(const uint64_t *p_words, uint16_t count)
{
    if(NULL == p_words || 3U != count) { return 0U; }
    memcpy(g_nv_data, p_words, 24U);
    return 1U;
}
HAL_StatusTypeDef HAL_RS485Ex_Init(UART_HandleTypeDef *p_handle,
                                  uint32_t polarity,
                                  uint32_t assertion, uint32_t deassertion)
{
    assert(NULL != p_handle && UART_DE_POLARITY_HIGH == polarity);
    assert(0U == assertion && 0U == deassertion);
    return HAL_OK;
}

static unsigned port_of(UART_HandleTypeDef *p_handle)
{
    unsigned i;
    for(i = 0U; i < 3U; i++)
    {
        if(p_handles[i] == p_handle)
        {
            return i;
        }
    }
    abort();
}
void test_disable(UART_HandleTypeDef *p_handle, uint32_t interrupt)
{
    (void)interrupt;
    p_rx[port_of(p_handle)] = NULL;
}
uint32_t HAL_GetTick(void) { return g_now; }
uint32_t bsp_system_time_ms(void) { return g_now; }
uint32_t bsp_system_irq_save(void)
{
    uint32_t previous = g_irq;
    g_irq = 1U;
    return previous;
}
void bsp_system_irq_restore(uint32_t previous) { g_irq = previous; }
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *p_handle,
                                     uint8_t *p_buffer, uint16_t length)
{
    assert(1U == length);
    if(0U != g_rx_fail)
    {
        g_rx_fail--;
        return HAL_ERROR;
    }
    p_rx[port_of(p_handle)] = p_buffer;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *p_handle,
                                      uint8_t *p_buffer, uint16_t length)
{
    unsigned port = port_of(p_handle);
    if(0U != g_force_busy)
    {
        return HAL_BUSY;
    }
    p_tx[port] = p_buffer;
    g_tx_len[port] = length;
    assert(g_tx_calls < sizeof(g_tx_log));
    g_tx_log[g_tx_calls++] = p_buffer[0U];
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *p_handle)
{
    p_rx[port_of(p_handle)] = NULL;
    p_handle->ErrorCode = 0U;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *p_handle)
{
    if(0U != g_abort_tx_fail)
    {
        g_abort_tx_fail--;
        return HAL_ERROR;
    }
    p_tx[port_of(p_handle)] = NULL;
    g_abort_count++;
    return HAL_OK;
}
static void inject(comm_port_t port, uint8_t value, uint32_t time_ms)
{
    uint8_t *p_byte = p_rx[port];
    assert(NULL != p_byte);
    g_now = time_ms;
    p_rx[port] = NULL;
    *p_byte = value;
    HAL_UART_RxCpltCallback(p_handles[port]);
}
static void complete(comm_port_t port)
{
    assert(NULL != p_tx[port]);
    p_tx[port] = NULL;
    HAL_UART_TxCpltCallback(p_handles[port]);
}
static void advance(uint32_t time_ms)
{
    g_now = time_ms;
    svc_serial_task();
}
static void expect_read(comm_port_t port, const uint8_t *p_expected,
                         uint16_t length)
{
    uint8_t buffer[COMM_SERIAL_FRAME_SIZE];
    uint16_t received = 0U;
    assert(SVC_SERIAL_OK ==
           svc_serial_read(port, buffer, sizeof(buffer), &received));
    assert(length == received);
    assert(0 == memcmp(buffer, p_expected, length));
}
static void expect_empty(comm_port_t port)
{
    uint8_t buffer[COMM_SERIAL_FRAME_SIZE];
    uint16_t length = 99U;
    assert(SVC_SERIAL_EMPTY ==
           svc_serial_read(port, buffer, sizeof(buffer), &length));
    assert(0U == length);
}
static svc_serial_stats_t stats(comm_port_t port)
{
    svc_serial_stats_t result;
    assert(1U == svc_serial_get_stats(port, &result));
    return result;
}
static void test_not_ready(void)
{
    uint8_t value = 1U;
    uint16_t length = 1U;
    assert(SVC_SERIAL_NOT_READY == svc_serial_send(COMM_PORT_4G, &value, 1U));
    assert(SVC_SERIAL_NOT_READY ==
           svc_serial_read(COMM_PORT_4G, &value, 1U, &length));
    assert(0U == length);
    assert(1U == svc_serial_init());
    assert(0U == svc_serial_init());
}
static void test_silence(void)
{
    const uint8_t expected[] = {0U, 0xFFU, 0x42U};
    inject(COMM_PORT_4G, expected[0U], 100U);
    inject(COMM_PORT_4G, expected[1U], 114U);
    inject(COMM_PORT_4G, expected[2U], 128U);
    advance(142U);
    expect_empty(COMM_PORT_4G);
    advance(143U);
    expect_read(COMM_PORT_4G, expected, sizeof(expected));
    assert(3U == stats(COMM_PORT_4G).rx_bytes);
}
static void test_irq_gap(void)
{
    uint8_t first = 1U, second = 2U;
    inject(COMM_PORT_RS485_1, first, 100U);
    inject(COMM_PORT_RS485_1, second, 150U);
    expect_read(COMM_PORT_RS485_1, &first, 1U);
    advance(200U);
    expect_read(COMM_PORT_RS485_1, &second, 1U);
}
static void test_oversize(void)
{
    uint8_t expected[COMM_SERIAL_FRAME_SIZE];
    unsigned i;
    memset(expected, 0xA5, sizeof(expected));
    for(i = 0U; i < COMM_SERIAL_FRAME_SIZE; i++)
    {
        inject(COMM_PORT_4G, expected[i], i);
    }
    advance(270U);
    expect_read(COMM_PORT_4G, expected, sizeof(expected));
    for(i = 0U; i <= COMM_SERIAL_FRAME_SIZE; i++)
    {
        inject(COMM_PORT_4G, 0x55U, 300U + i);
    }
    advance(571U);
    expect_empty(COMM_PORT_4G);
    assert(1U == stats(COMM_PORT_4G).rx_oversize);
    inject(COMM_PORT_4G, 0xA5U, 600U);
    advance(615U);
    expect_read(COMM_PORT_4G, expected, 1U);
}
static void test_rx_queue(void)
{
    unsigned i;
    uint8_t expected;
    for(i = 0U; i < COMM_SERIAL_QUEUE_DEPTH + 1U; i++)
    {
        inject(COMM_PORT_4G, (uint8_t)i, i * 20U);
        advance(i * 20U + 15U);
    }
    assert(1U == stats(COMM_PORT_4G).rx_queue_full);
    for(i = 0U; i < COMM_SERIAL_QUEUE_DEPTH; i++)
    {
        expected = (uint8_t)i;
        expect_read(COMM_PORT_4G, &expected, 1U);
    }
    expect_empty(COMM_PORT_4G);
    for(i = 0U; i < 100U; i++)
    {
        expected = (uint8_t)i;
        inject(COMM_PORT_4G, expected, 1000U + i * 20U);
        advance(1015U + i * 20U);
        expect_read(COMM_PORT_4G, &expected, 1U);
    }
}
static void test_small_buffer(void)
{
    const uint8_t expected[] = {1U, 2U};
    uint8_t buffer[2];
    uint16_t length;
    inject(COMM_PORT_4G, 1U, 0U);
    inject(COMM_PORT_4G, 2U, 1U);
    advance(16U);
    assert(SVC_SERIAL_BUFFER_SMALL ==
           svc_serial_read(COMM_PORT_4G, buffer, 1U, &length));
    assert(2U == length);
    expect_read(COMM_PORT_4G, expected, 2U);
    assert(SVC_SERIAL_INVALID ==
           svc_serial_read((comm_port_t)-1, buffer, 2U, &length));
    assert(0U == length);
    assert(SVC_SERIAL_INVALID == svc_serial_send(COMM_PORT_NUM, buffer, 1U));
    assert(SVC_SERIAL_INVALID == svc_serial_send(COMM_PORT_4G, NULL, 1U));
    assert(SVC_SERIAL_INVALID == svc_serial_send(COMM_PORT_4G, buffer, 0U));
}
static void test_rx_error(void)
{
    uint8_t expected = 4U;
    inject(COMM_PORT_RS485_2, 1U, 0U);
    g_now = 1U;
    huart3.ErrorCode = 1U;
    HAL_UART_ErrorCallback(&huart3);
    g_rx_fail = 1U;
    advance(1U);
    assert(NULL == p_rx[2U]);
    advance(10U);
    assert(NULL == p_rx[2U]);
    advance(11U);
    assert(NULL != p_rx[2U]);
    inject(COMM_PORT_RS485_2, 2U, 12U);
    advance(62U);
    expect_empty(COMM_PORT_RS485_2);
    inject(COMM_PORT_RS485_2, expected, 70U);
    advance(120U);
    expect_read(COMM_PORT_RS485_2, &expected, 1U);
    assert(1U == stats(COMM_PORT_RS485_2).rx_errors);
}
static void test_tx_queue(void)
{
    unsigned i;
    uint8_t value;
    for(i = 0U; i < COMM_SERIAL_QUEUE_DEPTH; i++)
    {
        value = (uint8_t)(i + 1U);
        assert(SVC_SERIAL_OK == svc_serial_send(COMM_PORT_4G, &value, 1U));
    }
    value = 99U;
    assert(SVC_SERIAL_FULL == svc_serial_send(COMM_PORT_4G, &value, 1U));
    assert(1U == stats(COMM_PORT_4G).tx_queue_full);
    svc_serial_task();
    assert(1U == *p_tx[0U]);
    for(i = 0U; i < COMM_SERIAL_QUEUE_DEPTH; i++)
    {
        assert((uint8_t)(i + 1U) == *p_tx[0U]);
        complete(COMM_PORT_4G);
        svc_serial_task();
        svc_serial_task();
    }
    assert(COMM_SERIAL_QUEUE_DEPTH == stats(COMM_PORT_4G).tx_frames);
    assert(COMM_SERIAL_QUEUE_DEPTH == g_tx_calls);
    for(i = 0U; i < COMM_SERIAL_QUEUE_DEPTH; i++)
    {
        assert((uint8_t)(i + 1U) == g_tx_log[i]);
    }
}
static void test_tx_timeout(void)
{
    uint8_t value = 7U;
    assert(SVC_SERIAL_OK == svc_serial_send(COMM_PORT_4G, &value, 1U));
    advance(0U);
    advance(100U);
    assert(0U == g_abort_count);
    g_abort_tx_fail = 1U;
    advance(101U);
    assert(0U == stats(COMM_PORT_4G).tx_errors);
    advance(102U);
    assert(1U == g_abort_count);
    assert(1U == stats(COMM_PORT_4G).tx_errors);
    assert(SVC_SERIAL_OK == svc_serial_send(COMM_PORT_4G, &value, 1U));
    advance(103U);
    complete(COMM_PORT_4G);
    advance(104U);
    assert(1U == stats(COMM_PORT_4G).tx_frames);
}
static void test_start_busy(void)
{
    uint8_t value = 3U;
    g_force_busy = 1U;
    assert(SVC_SERIAL_OK == svc_serial_send(COMM_PORT_4G, &value, 1U));
    advance(0U);
    advance(999U);
    assert(0U == stats(COMM_PORT_4G).tx_errors);
    advance(1000U);
    assert(1U == stats(COMM_PORT_4G).tx_errors);
    assert(0U == g_tx_calls);
}
static void test_rollover(void)
{
    uint8_t value = 9U;
    inject(COMM_PORT_4G, value, UINT32_MAX - 5U);
    advance(8U);
    expect_empty(COMM_PORT_4G);
    advance(9U);
    expect_read(COMM_PORT_4G, &value, 1U);
    g_now = UINT32_MAX - 5U;
    assert(SVC_SERIAL_OK == svc_serial_send(COMM_PORT_4G, &value, 1U));
    svc_serial_task();
    advance(94U);
    assert(0U == stats(COMM_PORT_4G).tx_errors);
    advance(95U);
    assert(1U == stats(COMM_PORT_4G).tx_errors);
}
static void test_ports(void)
{
    unsigned i;
    uint8_t value;
    for(i = 0U; i < 3U; i++)
    {
        value = (uint8_t)(i + 10U);
        inject((comm_port_t)i, value, 100U);
        assert(SVC_SERIAL_OK ==
               svc_serial_send((comm_port_t)i, &value, 1U));
    }
    advance(150U);
    for(i = 0U; i < 3U; i++)
    {
        value = (uint8_t)(i + 10U);
        expect_read((comm_port_t)i, &value, 1U);
        assert(value == *p_tx[i]);
        assert(1U == g_tx_len[i]);
        complete((comm_port_t)i);
    }
    svc_serial_task();
    for(i = 0U; i < 3U; i++)
    {
        assert(1U == stats((comm_port_t)i).tx_frames);
    }
}
static void test_rearm_failure(void)
{
    uint8_t value = 8U;
    g_rx_fail = 1U;
    inject(COMM_PORT_4G, 1U, 0U);
    advance(0U);
    advance(15U);
    expect_empty(COMM_PORT_4G);
    inject(COMM_PORT_4G, value, 20U);
    advance(35U);
    expect_read(COMM_PORT_4G, &value, 1U);
    assert(1U == stats(COMM_PORT_4G).rx_errors);
}
int main(int argc, char **argv)
{
    int scenario;
    assert(2 == argc);
    scenario = atoi(argv[1]);
    if(0 == scenario)
    {
        test_not_ready();
        return 0;
    }
    assert(1U == svc_serial_init());
    switch(scenario)
    {
        case 1: test_silence(); break;
        case 2: test_irq_gap(); break;
        case 3: test_oversize(); break;
        case 4: test_rx_queue(); break;
        case 5: test_small_buffer(); break;
        case 6: test_rx_error(); break;
        case 7: test_tx_queue(); break;
        case 8: test_tx_timeout(); break;
        case 9: test_start_busy(); break;
        case 10: test_rollover(); break;
        case 11: test_ports(); break;
        case 12: test_rearm_failure(); break;
        default: abort();
    }
    /* API calls preserve pre-existing interrupt guards. */
    g_irq = 1U;
    svc_serial_task();
    (void)stats(COMM_PORT_4G);
    assert(1U == g_irq);
    return 0;
}
"""

class SerialTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("gcc")
        if compiler is None:
            raise RuntimeError("Native GCC is required in PATH")
        cls.temp = tempfile.TemporaryDirectory(prefix="comm-convert-serial-")
        cls.addClassCleanup(cls.temp.cleanup)
        directory = pathlib.Path(cls.temp.name)
        (directory / "usart.h").write_text(HAL_HEADER, encoding="utf-8")
        harness = directory / "serial_harness.c"
        harness.write_text(HARNESS, encoding="utf-8")
        cls.executable = directory / "serial_test.exe"
        command = [compiler, "-std=c99", "-Wall", "-Wextra", "-Werror",
                   "-I", str(directory), "-I", str(ROOT / "Config"),
                   "-I", str(ROOT / "Bsp"), "-I", str(ROOT / "Services"),
                   str(ROOT / "Bsp/bsp_uart.c"),
                   str(ROOT / "Services/svc_serial.c"), str(harness),
                   str(ROOT / "Config/comm_params.c"),
                   "-o", str(cls.executable)]
        subprocess.run(command, check=True, cwd=ROOT, timeout=60)

    def test_regressions(self):
        scenarios = [
            "initialization", "silence resets per byte", "IRQ gap boundary",
            "exact length and oversize recovery", "queue full and wrap",
            "small buffer and invalid inputs", "RX fault and recovery retry",
            "TX order and copied buffer", "TX timeout and abort retry",
            "bounded busy before start", "millisecond counter rollover",
            "three independent ports", "RX rearm failure",
        ]
        for index, name in enumerate(scenarios):
            with self.subTest(scenario=name):
                subprocess.run([str(self.executable), str(index)],
                               check=True, timeout=10, cwd=ROOT)
                print("PASS:", name)

if __name__ == "__main__":
    unittest.main(verbosity=2)
