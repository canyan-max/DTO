
"""Local Modbus parameter and deferred-save integration with actual App/BSP."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

from test_serial import HAL_HEADER, HARNESS, ROOT
from test_dto import DTO_HARNESS

LOCAL_HARNESS = r"""
#include "app_local.h"
#include "proto_local.h"
#include "svc_params.h"
#include "params_config.h"

static app_local_status_t local_status(void)
{
    app_local_status_t status;
    assert(1U == app_local_get_status(&status));
    return status;
}
static comm_params_t params(void)
{
    comm_params_t value;
    assert(1U == svc_params_get(&value));
    return value;
}
static uint16_t reg16(const uint8_t *p_data)
{
    return ((uint16_t)p_data[0U]<<8U)|p_data[1U];
}
static void crc_frame(uint8_t *p_data, uint16_t payload)
{
    uint16_t crc=proto_local_crc16(p_data,payload);
    p_data[payload]=(uint8_t)crc;
    p_data[payload+1U]=(uint8_t)(crc>>8U);
}
static void send_request(uint8_t *p_data,uint16_t length,uint32_t base)
{
    unsigned i;
    crc_frame(p_data,length-2U);
    for(i=0U;i<length;i++)
    {
        inject(COMM_PORT_4G,p_data[i],base+i);
    }
    g_now=base+length-1U+15U;
    dto_step();
    dto_step();
    dto_step();
}
static void finish_reply(void)
{
    assert(NULL!=p_tx[0U]);
    complete(COMM_PORT_4G);
    dto_step();
}
static void load_saved(void)
{
    uint32_t sum;
    g_nv_data[0U]=(uint64_t)19200U|((uint64_t)38400U<<32U);
    g_nv_data[1U]=(uint64_t)10500U|((uint64_t)12500U<<16U);
    sum=(uint32_t)g_nv_data[0U]^(uint32_t)(g_nv_data[0U]>>32U)^
        (uint32_t)g_nv_data[1U]^(uint32_t)(g_nv_data[1U]>>32U);
    g_nv_data[2U]=(uint64_t)PARAMS_VALID_MAGIC|((uint64_t)sum<<32U);
}
static void test_read(void)
{
    uint8_t request[]={0x14U,3U,0U,26U,0U,8U,0U,0U};
    uint8_t other[]={1U,3U,0U,0U,0U,1U,0U,0U};
    send_request(request,sizeof(request),100U);
    assert(21U==g_tx_len[0U]);
    assert(0x14U==p_tx[0U][0U] && 3U==p_tx[0U][1U]);
    assert(16U==p_tx[0U][2U]);
    assert(10000U==reg16(p_tx[0U]+3U));
    assert(12000U==reg16(p_tx[0U]+5U));
    assert(0U==reg16(p_tx[0U]+7U));
    assert(9600U==reg16(p_tx[0U]+11U));
    assert(9600U==reg16(p_tx[0U]+15U));
    assert(NULL==p_tx[1U] && NULL==p_tx[2U]);
    finish_reply();
    assert(APP_LOCAL_DONE==local_status().state);
    assert(0U==g_nv_writes && 0U==g_reset_count);
    send_request(other,sizeof(other),200U);
    assert(NULL==p_tx[0U] && NULL!=p_tx[1U] && NULL!=p_tx[2U]);
    assert(0==memcmp(other,p_tx[1U],sizeof(other)));
}
static void test_write_save(void)
{
    uint8_t change[]={0x14U,6U,0U,26U,0x29U,0x04U,0U,0U};
    uint8_t save[]={0x14U,6U,0U,33U,0xDDU,0xCDU,0U,0U};
    send_request(change,sizeof(change),100U);
    assert(10500U==params().uv_protect_mv);
    assert(0==memcmp(change,p_tx[0U],sizeof(change)));
    finish_reply();
    assert(0U==g_nv_writes && 0U==g_reset_count);
    send_request(save,sizeof(save),200U);
    assert(0U==g_nv_writes && 0U==g_reset_count);
    assert(0==memcmp(save,p_tx[0U],sizeof(save)));
    assert(NULL!=p_rx[0U]);
    finish_reply();
    assert(1U==g_nv_writes && 1U==g_reset_count);
    assert(NULL==p_rx[0U] && NULL==p_rx[1U] && NULL==p_rx[2U]);
    assert(APP_LOCAL_RESET_REQUESTED==local_status().state);
    dto_step();
    assert(1U==g_nv_writes && 1U==g_reset_count);
    svc_params_init();
    assert(10500U==params().uv_protect_mv);
}
static void test_baud_pairs(void)
{
    uint8_t request[]={0x14U,0x10U,0U,29U,0U,4U,8U,
                      0U,0U,0x4BU,0U,0U,0U,0U,1U,0U,0U};
    send_request(request,sizeof(request),100U);
    assert(0x90U==p_tx[0U][1U] && 3U==p_tx[0U][2U]);
    assert(9600U==params().rs485_1_baud && 9600U==params().rs485_2_baud);
    finish_reply();
    request[13U]=0x96U;
    request[14U]=0U;
    send_request(request,sizeof(request),200U);
    assert(0x10U==p_tx[0U][1U] && 8U==g_tx_len[0U]);
    assert(19200U==params().rs485_1_baud && 38400U==params().rs485_2_baud);
    assert(9600U==huart2.Init.BaudRate && 9600U==huart3.Init.BaudRate);
    finish_reply();
    assert(0U==g_nv_writes);
}
static void test_invalid(void)
{
    uint8_t bad[]={0x14U,6U,0U,28U,0U,2U,0U,0U};
    uint8_t range[]={0x14U,3U,0U,16U,0U,2U,0U,0U};
    uint8_t half[]={0x14U,6U,0U,29U,0U,1U,0U,0U};
    send_request(bad,sizeof(bad),100U);
    assert(0x86U==p_tx[0U][1U] && 3U==p_tx[0U][2U]);
    assert(0U==params().uv_enabled);
    finish_reply();
    send_request(range,sizeof(range),200U);
    assert(0x83U==p_tx[0U][1U] && 2U==p_tx[0U][2U]);
    finish_reply();
    send_request(half,sizeof(half),300U);
    assert(0x86U==p_tx[0U][1U] && 2U==p_tx[0U][2U]);
    finish_reply();
    assert(0U==g_nv_writes && 0U==g_reset_count);
}
static void test_defaults_command(void)
{
    uint8_t request[]={0x14U,6U,0U,33U,0xCCU,0xDDU,0U,0U};
    assert(19200U==params().rs485_1_baud);
    send_request(request,sizeof(request),100U);
    assert(0U==g_nv_writes && 0U==g_reset_count);
    finish_reply();
    assert(1U==g_nv_writes && 1U==g_reset_count);
    svc_params_init();
    assert(9600U==params().rs485_1_baud && 9600U==params().rs485_2_baud);
    assert(10000U==params().uv_protect_mv);
}
static void test_bad_crc(void)
{
    uint8_t request[]={0x14U,6U,0U,26U,0x29U,4U,0U,0U};
    unsigned i;
    /* Bad CRC is swallowed as a local frame, never broadcast. */
    for(i=0U;i<sizeof(request);i++)
    {
        inject(COMM_PORT_4G,request[i],100U+i);
    }
    g_now=122U;
    dto_step(); dto_step(); dto_step();
    assert(NULL==p_tx[0U] && NULL==p_tx[1U] && NULL==p_tx[2U]);
    assert(10000U==params().uv_protect_mv);
    assert(1U==local_status().dropped);
}
static void test_queued_ack(void)
{
    uint8_t filler=0x90U;
    uint8_t request[]={0x14U,6U,0U,33U,0xDDU,0xCDU,0U,0U};
    unsigned i;
    for(i=0U;i<4U;i++)
    {
        assert(SVC_SERIAL_OK==svc_serial_send(COMM_PORT_4G,&filler,1U));
    }
    send_request(request,sizeof(request),100U);
    assert(APP_LOCAL_REPLY_PENDING==local_status().state);
    assert(0U==g_nv_writes);
    for(i=0U;i<4U;i++)
    {
        assert(NULL!=p_tx[0U] && filler==*p_tx[0U]);
        complete(COMM_PORT_4G);
        dto_step(); dto_step();
    }
    assert(NULL!=p_tx[0U] && 0x14U==*p_tx[0U]);
    assert(0U==g_nv_writes && 0U==g_reset_count);
    finish_reply();
    assert(1U==g_nv_writes && 1U==g_reset_count);
}
static void test_failed_reply(void)
{
    uint8_t request[]={0x14U,6U,0U,33U,0xDDU,0xCDU,0U,0U};
    g_force_busy=1U;
    send_request(request,sizeof(request),100U);
    g_now+=1000U;
    dto_step();
    assert(APP_LOCAL_FAILED==local_status().state);
    assert(0U==g_nv_writes && 0U==g_reset_count);
    assert(NULL!=p_rx[0U] && NULL!=p_rx[1U] && NULL!=p_rx[2U]);
}
static void test_failed_flash(void)
{
    uint8_t request[]={0x14U,6U,0U,33U,0xDDU,0xCDU,0U,0U};
    g_nv_fail=1U;
    send_request(request,sizeof(request),100U);
    finish_reply();
    assert(APP_LOCAL_FAILED==local_status().state);
    assert(1U==g_nv_writes && 0U==g_reset_count);
    assert(NULL!=p_rx[0U] && NULL!=p_rx[1U] && NULL!=p_rx[2U]);
    dto_step(); dto_step();
    assert(1U==g_nv_writes);
}
static void test_all_tx_drain(void)
{
    uint8_t filler=1U;
    uint8_t request[]={0x14U,6U,0U,33U,0xDDU,0xCDU,0U,0U};
    assert(SVC_SERIAL_OK==svc_serial_send(COMM_PORT_RS485_1,&filler,1U));
    send_request(request,sizeof(request),100U);
    assert(NULL!=p_tx[1U]);
    finish_reply();
    assert(0U==g_nv_writes && 0U==g_reset_count);
    complete(COMM_PORT_RS485_1);
    dto_step();
    assert(1U==g_nv_writes && 1U==g_reset_count);
}
static void test_suspend_rx(void)
{
    uint8_t value=1U;
    assert(1U==bsp_uart_suspend_rx(1U));
    assert(NULL==p_rx[0U] && NULL==p_rx[1U] && NULL==p_rx[2U]);
    svc_serial_task();
    assert(NULL==p_rx[0U]);
    assert(1U==bsp_uart_suspend_rx(0U));
    assert(1U==bsp_uart_suspend_rx(0U));
    assert(NULL!=p_rx[0U] && NULL!=p_rx[1U] && NULL!=p_rx[2U]);
    assert(0U==bsp_uart_suspend_rx(2U));
    assert(SVC_SERIAL_OK==svc_serial_send(COMM_PORT_4G,&value,1U));
    svc_serial_task();
    assert(0U==bsp_uart_suspend_rx(1U));
    assert(NULL!=p_rx[0U]);
}
static void test_codec_bounds(void)
{
    proto_local_snapshot_t snapshot={0};
    proto_local_command_t command;
    uint8_t out[PROTO_LOCAL_MAX_REPLY];
    uint8_t request[]={0x14U,6U,0U,26U,0x29U,4U,0U,0U};
    uint16_t length=99U;
    snapshot.params=params();
    crc_frame(request,6U);
    assert(0x4B37U==proto_local_crc16((const uint8_t *)"123456789",9U));
    assert(PROTO_LOCAL_DROP==proto_local_process(
        request,8U,&snapshot,&command,out,7U,&length));
    assert(0U==length);
    assert(PROTO_LOCAL_DROP==proto_local_process(
        request,7U,&snapshot,&command,out,sizeof(out),&length));
    assert(PROTO_LOCAL_NOT_LOCAL==proto_local_process(
        NULL,0U,&snapshot,&command,out,sizeof(out),&length));
}
int main(int argc,char **argv)
{
    int scenario;
    assert(2==argc);
    scenario=atoi(argv[1]);
    test_nv_reset();
    if(4==scenario) { load_saved(); }
    assert(1U==app_init());
    switch(scenario)
    {
        case 0: test_read(); break;
        case 1: test_write_save(); break;
        case 2: test_baud_pairs(); break;
        case 3: test_invalid(); break;
        case 4: test_defaults_command(); break;
        case 5: test_bad_crc(); break;
        case 6: test_queued_ack(); break;
        case 7: test_failed_reply(); break;
        case 8: test_failed_flash(); break;
        case 9: test_all_tx_drain(); break;
        case 10: test_suspend_rx(); break;
        case 11: test_codec_bounds(); break;
        default: abort();
    }
    return 0;
}
"""

class LocalTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler=shutil.which("gcc")
        if compiler is None: raise RuntimeError("Native GCC required")
        cls.temp=tempfile.TemporaryDirectory(prefix="comm-convert-local-")
        cls.addClassCleanup(cls.temp.cleanup)
        directory=pathlib.Path(cls.temp.name)
        (directory/"usart.h").write_text(HAL_HEADER,encoding="utf-8")
        source=HARNESS.replace("int main(int argc, char **argv)",
                               "int serial_base_main(int argc, char **argv)")
        source=source.replace("static uint8_t g_nv_ready;",
                              """static uint8_t g_nv_ready;
static unsigned g_nv_writes;
static uint8_t g_nv_fail;""")
        source=source.replace(
            "memcpy(g_nv_data, p_words, 24U);",
            """assert(NULL==p_rx[0U] && NULL==p_rx[1U] && NULL==p_rx[2U]);
    g_nv_writes++;
    if(0U!=g_nv_fail) { return 0U; }
    memcpy(g_nv_data,p_words,24U);""")
        source+=DTO_HARNESS.replace("int main(int argc, char **argv)",
                                    "int dto_base_main(int argc, char **argv)")
        harness=directory/"local_harness.c"
        harness.write_text(source+LOCAL_HARNESS,encoding="utf-8")
        cls.executable=directory/"local_test.exe"
        command=[compiler, '-DCOMM_APP_ENABLE_MODE=0',"-DCOMM_APP_ENABLE_DTO=1","-DCOMM_APP_ENABLE_HIK=0","-std=c99","-Wall","-Wextra","-Werror"]
        for include in (directory,ROOT/"Config",ROOT/"Bsp",ROOT/"Services",
                        ROOT/"App",ROOT/"Protocols"):
            command+=["-I",str(include)]
        for path in ("Bsp/bsp_uart.c","Services/svc_serial.c",
                     "Protocols/proto_gm800.c","Services/svc_gm800.c",
                     "Services/svc_params.c", "Services/svc_boot.c","Config/comm_params.c",
                     "App/app_dto.c","App/app_main.c",
                     "Services/svc_identity.c", "Services/svc_relay.c", "Services/svc_telemetry.c", "App/app_local.c","Protocols/proto_local.c"):
            command.append(str(ROOT/path))
        command += [str(harness),"-o",str(cls.executable)]
        subprocess.run(command,check=True,cwd=ROOT,timeout=60)
    def test_local_protocol(self):
        scenarios=[
            "read registers locally, other addresses still broadcast",
            "RAM write then ACK-complete save and one reset",
            "paired baud writes are atomic and deferred to reboot",
            "invalid enable, range and half-baud writes rejected",
            "restore defaults after ACK, then reset",
            "bad CRC never replies, modifies or broadcasts",
            "congested UART1 keeps ACK queued before saving",
            "failed reply prevents Flash write and reset",
            "Flash failure resumes RX without retry or reset",
            "save waits for all three TX queues",
            "RX suspension, resume and TX-busy protection",
            "codec boundaries and independent CRC check vector",
        ]
        for index,name in enumerate(scenarios):
            with self.subTest(scenario=name):
                subprocess.run([str(self.executable),str(index)],check=True,
                               timeout=10,cwd=ROOT)
                print("PASS:",name)
if __name__=="__main__":
    unittest.main(verbosity=2)
