"""DTO integration tests using actual App, Services and UART BSP with fake HAL."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

from test_serial import HAL_HEADER, HARNESS, ROOT

DTO_HARNESS = r"""
#include "app_dto.h"
#include "app_main.h"

static uint32_t g_feed_count;
uint8_t bsp_io_set_power(uint8_t a, uint8_t b, uint8_t c)
{
    assert(1U == a && 1U == b && 1U == c);
    return 1U;
}
uint8_t bsp_system_feed_watchdog(void)
{
    g_feed_count++;
    return 1U;
}
static void dto_step(void)
{
    assert(1U == app_task());
}
static app_dto_stats_t dto_stats(void)
{
    app_dto_stats_t result;
    assert(1U == app_dto_get_stats(&result));
    return result;
}
static void test_dto_broadcast(void)
{
    const uint8_t request[] = {0U, 0xFFU, 0x42U, 'A', 0U};
    unsigned i;
    for(i = 0U; i < sizeof(request); i++)
    {
        inject(COMM_PORT_4G, request[i], 100U + i);
    }
    advance(119U);
    dto_step();
    svc_serial_task();
    assert(NULL == p_tx[0U]);
    for(i = 1U; i < 3U; i++)
    {
        assert(sizeof(request) == g_tx_len[i]);
        assert(0 == memcmp(request, p_tx[i], sizeof(request)));
        complete((comm_port_t)i);
    }
    svc_serial_task();
    dto_step();
    assert(2U == g_tx_calls);
    assert(1U == dto_stats().downlink_queued);
    assert(0U == dto_stats().errors);
    assert(2U == g_feed_count);
}
static void test_dto_replies(void)
{
    const uint8_t a[] = {0xA1U, 0U, 0xFFU};
    const uint8_t b[] = {0xB1U, 0x42U, 0U};
    unsigned i;
    for(i = 0U; i < sizeof(a); i++)
    {
        inject(COMM_PORT_RS485_1, a[i], 100U + i);
        inject(COMM_PORT_RS485_2, b[i], 100U + i);
    }
    advance(152U);
    dto_step();
    svc_serial_task();
    assert(0 == memcmp(p_tx[0U], a, sizeof(a)));
    assert(sizeof(a) == g_tx_len[0U]);
    assert(NULL == p_tx[1U] && NULL == p_tx[2U]);
    complete(COMM_PORT_4G);
    svc_serial_task();
    svc_serial_task();
    assert(0 == memcmp(p_tx[0U], b, sizeof(b)));
    assert(sizeof(b) == g_tx_len[0U]);
    complete(COMM_PORT_4G);
    svc_serial_task();
    dto_step();
    assert(2U == g_tx_calls);
    assert(1U == dto_stats().uplink_queued[0U]);
    assert(1U == dto_stats().uplink_queued[1U]);
}
static void test_atomic_mask(void)
{
    const uint32_t mask = (1U << COMM_PORT_RS485_1) |
                          (1U << COMM_PORT_RS485_2);
    uint8_t filler = 0x90U, request = 0x55U;
    unsigned i;
    for(i = 0U; i < COMM_SERIAL_QUEUE_DEPTH; i++)
    {
        assert(SVC_SERIAL_OK ==
               svc_serial_send(COMM_PORT_RS485_2, &filler, 1U));
    }
    assert(SVC_SERIAL_FULL == svc_serial_send_mask(mask, &request, 1U));
    svc_serial_task();
    assert(NULL == p_tx[1U]);
    complete(COMM_PORT_RS485_2);
    svc_serial_task();
    assert(SVC_SERIAL_OK == svc_serial_send_mask(mask, &request, 1U));
    svc_serial_task();
    assert(request == *p_tx[1U]);
    complete(COMM_PORT_RS485_1);
    for(i = 0U; i < COMM_SERIAL_QUEUE_DEPTH; i++)
    {
        if(i == COMM_SERIAL_QUEUE_DEPTH - 1U)
        {
            assert(request == *p_tx[2U]);
        }
        else
        {
            assert(filler == *p_tx[2U]);
        }
        complete(COMM_PORT_RS485_2);
        svc_serial_task();
        svc_serial_task();
    }
    assert(1U == stats(COMM_PORT_RS485_1).tx_frames);
    assert(COMM_SERIAL_QUEUE_DEPTH + 1U ==
           stats(COMM_PORT_RS485_2).tx_frames);
}
static void test_pending_fifo(void)
{
    uint8_t filler = 0x90U;
    unsigned i;
    for(i = 0U; i < COMM_SERIAL_QUEUE_DEPTH; i++)
    {
        assert(SVC_SERIAL_OK ==
               svc_serial_send(COMM_PORT_4G, &filler, 1U));
    }
    inject(COMM_PORT_RS485_1, 0xA1U, 0U);
    advance(50U);
    dto_step();
    assert(0U == dto_stats().uplink_queued[0U]);
    inject(COMM_PORT_RS485_1, 0xA2U, 51U);
    advance(101U);
    dto_step();
    assert(0U == dto_stats().uplink_queued[0U]);
    for(i = 0U; i < COMM_SERIAL_QUEUE_DEPTH; i++)
    {
        assert(filler == *p_tx[0U]);
        complete(COMM_PORT_4G);
        svc_serial_task();
        svc_serial_task();
    }
    assert(NULL == p_tx[0U]);
    dto_step();
    dto_step();
    assert(2U == dto_stats().uplink_queued[0U]);
    svc_serial_task();
    assert(0xA1U == *p_tx[0U]);
    complete(COMM_PORT_4G);
    svc_serial_task();
    svc_serial_task();
    assert(0xA2U == *p_tx[0U]);
    complete(COMM_PORT_4G);
    svc_serial_task();
    dto_step();
    assert(COMM_SERIAL_QUEUE_DEPTH + 2U == g_tx_calls);
}
static void test_reply_fairness(void)
{
    uint8_t filler = 0x90U;
    unsigned i;
    for(i = 0U; i < COMM_SERIAL_QUEUE_DEPTH; i++)
    {
        assert(SVC_SERIAL_OK ==
               svc_serial_send(COMM_PORT_4G, &filler, 1U));
    }
    inject(COMM_PORT_RS485_1, 0xA1U, 0U);
    inject(COMM_PORT_RS485_2, 0xB1U, 0U);
    advance(50U);
    dto_step();
    complete(COMM_PORT_4G);
    svc_serial_task();
    dto_step();
    assert(1U == dto_stats().uplink_queued[0U]);
    assert(0U == dto_stats().uplink_queued[1U]);
    inject(COMM_PORT_RS485_1, 0xA2U, 51U);
    advance(101U);
    complete(COMM_PORT_4G);
    svc_serial_task();
    dto_step();
    assert(1U == dto_stats().uplink_queued[0U]);
    assert(1U == dto_stats().uplink_queued[1U]);
    assert(0U == dto_stats().errors);
}
static void test_dto_boundary(void)
{
    uint8_t frame[COMM_SERIAL_FRAME_SIZE];
    unsigned i;
    for(i = 0U; i < sizeof(frame); i++)
    {
        frame[i] = (uint8_t)i;
        inject(COMM_PORT_4G, frame[i], i);
    }
    advance(COMM_SERIAL_FRAME_SIZE - 1U + COMM_UART1_RX_IDLE_MS);
    dto_step();
    svc_serial_task();
    for(i = 1U; i < 3U; i++)
    {
        assert(sizeof(frame) == g_tx_len[i]);
        assert(0 == memcmp(frame, p_tx[i], sizeof(frame)));
        complete((comm_port_t)i);
    }
    svc_serial_task();
    for(i = 0U; i <= sizeof(frame); i++)
    {
        inject(COMM_PORT_4G, 1U, 1000U + i);
    }
    advance(1000U + COMM_SERIAL_FRAME_SIZE + COMM_UART1_RX_IDLE_MS);
    dto_step();
    svc_serial_task();
    assert(1U == dto_stats().downlink_queued);
    assert(1U == stats(COMM_PORT_4G).rx_oversize);
    assert(2U == g_tx_calls);
}
static void test_mask_inputs(void)
{
    uint8_t value = 1U;
    assert(SVC_SERIAL_INVALID == svc_serial_send_mask(0U, &value, 1U));
    assert(SVC_SERIAL_INVALID ==
           svc_serial_send_mask(1U << COMM_PORT_NUM, &value, 1U));
    assert(SVC_SERIAL_INVALID == svc_serial_send_mask(1U, NULL, 1U));
    assert(SVC_SERIAL_INVALID == svc_serial_send_mask(1U, &value, 0U));
    assert(SVC_SERIAL_INVALID ==
           svc_serial_send_mask(1U, &value, COMM_SERIAL_FRAME_SIZE + 1U));
    svc_serial_task();
    assert(0U == g_tx_calls);
    assert(0U == app_dto_get_stats(NULL));
}
static void test_downlink_pending(void)
{
    uint8_t filler = 0x90U;
    unsigned i;
    for(i = 0U; i < COMM_SERIAL_QUEUE_DEPTH; i++)
    {
        assert(SVC_SERIAL_OK ==
               svc_serial_send(COMM_PORT_RS485_2, &filler, 1U));
    }
    inject(COMM_PORT_4G, 0x11U, 0U);
    advance(15U);
    dto_step();
    svc_serial_task();
    assert(NULL == p_tx[1U]);
    assert(0U == dto_stats().downlink_queued);
    inject(COMM_PORT_4G, 0x22U, 20U);
    advance(35U);
    dto_step();
    for(i = 0U; i < COMM_SERIAL_QUEUE_DEPTH; i++)
    {
        complete(COMM_PORT_RS485_2);
        svc_serial_task();
        svc_serial_task();
    }
    dto_step();
    dto_step();
    assert(2U == dto_stats().downlink_queued);
    svc_serial_task();
    assert(0x11U == *p_tx[1U] && 0x11U == *p_tx[2U]);
    complete(COMM_PORT_RS485_1);
    complete(COMM_PORT_RS485_2);
    svc_serial_task();
    svc_serial_task();
    assert(0x22U == *p_tx[1U] && 0x22U == *p_tx[2U]);
}
int main(int argc, char **argv)
{
    int scenario;
    assert(2 == argc);
    scenario = atoi(argv[1]);
    assert(1U == app_init());
    switch(scenario)
    {
        case 0: test_dto_broadcast(); break;
        case 1: test_dto_replies(); break;
        case 2: test_atomic_mask(); break;
        case 3: test_pending_fifo(); break;
        case 4: test_reply_fairness(); break;
        case 5: test_dto_boundary(); break;
        case 6: test_mask_inputs(); break;
        case 7: test_downlink_pending(); break;
        default: abort();
    }
    return 0;
}
"""

class DtoTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("gcc")
        if compiler is None:
            raise RuntimeError("Native GCC is required")
        cls.temp = tempfile.TemporaryDirectory(prefix="comm-convert-dto-")
        cls.addClassCleanup(cls.temp.cleanup)
        directory = pathlib.Path(cls.temp.name)
        (directory / "usart.h").write_text(HAL_HEADER, encoding="utf-8")
        harness = directory / "dto_harness.c"
        source = HARNESS.replace("int main(int argc, char **argv)",
                                 "int serial_base_main(int argc, char **argv)")
        harness.write_text(source + DTO_HARNESS, encoding="utf-8")
        cls.executable = directory / "dto_test.exe"
        command = [compiler, '-DCOMM_APP_ENABLE_MODE=0',"-DCOMM_APP_ENABLE_DTO=1","-DCOMM_APP_ENABLE_HIK=0", "-std=c99", "-Wall", "-Wextra", "-Werror"]
        for include in (directory, ROOT / "Config", ROOT / "Bsp",
                        ROOT / "Services", ROOT / "App", ROOT / "Protocols"):
            command += ["-I", str(include)]
        command += [str(ROOT / path) for path in
                    ("Bsp/bsp_uart.c", "Services/svc_serial.c",
                     "App/app_dto.c", "App/app_main.c",
                     "Protocols/proto_gm800.c", "Services/svc_gm800.c",
                     "Services/svc_params.c", "Services/svc_boot.c", "Config/comm_params.c",
                     "Services/svc_identity.c", "Services/svc_relay.c", "Services/svc_telemetry.c", "App/app_local.c", "Protocols/proto_local.c")]
        command += [str(harness), "-o", str(cls.executable)]
        subprocess.run(command, check=True, cwd=ROOT, timeout=60)

    def test_dto_paths(self):
        scenarios = [
            "broadcast binary data and no echo",
            "two concurrent replies preserve whole frames",
            "broadcast all queues or none",
            "pending reply FIFO survives upstream congestion",
            "round-robin reply fairness with one free slot",
            "256-byte forwarding and oversize discard",
            "invalid broadcast masks",
            "pending broadcasts retain order and are not duplicated",
        ]
        for index, name in enumerate(scenarios):
            with self.subTest(scenario=name):
                subprocess.run([str(self.executable), str(index)],
                               check=True, timeout=10, cwd=ROOT)
                print("PASS:", name)

if __name__ == "__main__":
    unittest.main(verbosity=2)
