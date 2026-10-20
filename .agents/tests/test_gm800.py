
"""Legacy GM800 sequence and DTO arbitration integration tests with fake HAL."""
import json
import pathlib
import shutil
import subprocess
import tempfile
import unittest

from test_serial import HAL_HEADER, HARNESS, ROOT
from test_dto import DTO_HARNESS

GM_HARNESS = r"""
#include "svc_gm800.h"
#include "proto_gm800.h"
#include "gm800_config.h"
static comm_port_t g_test_port;
static svc_gm800_status_t gm_status(void)
{
 svc_gm800_status_t status; assert(svc_gm800_get_status(&status)); return status;
}
static void pump(uint32_t now) {g_now=now; assert(app_task());}
static void flush_logs(void)
{
 unsigned n;
 for(n=0;n<8;n++)
 {
  pump(g_now);
  if(p_tx[g_test_port]) complete(g_test_port);
 }
 pump(g_now);
}
static void reply(const char *p_text)
{
 unsigned i; uint32_t now=g_now+20U;
 for(i=0;i<strlen(p_text);i++) inject(COMM_PORT_4G,(uint8_t)p_text[i],now+i);
 pump(now+i+16U); flush_logs();
}
static void begin(comm_port_t port)
{
 const char *p_request="AT+CLOUD=12345678901234567890";
 unsigned i; uint32_t now=g_now+100U;
 g_test_port=port;
 for(i=0;i<strlen(p_request);i++) inject(port,p_request[i],now+i);
 pump(now+i+50U); assert(SVC_GM800_DRAINING==gm_status().state);
 pump(g_now); assert(SVC_GM800_SENDING==gm_status().state);
}
static void issue(unsigned step)
{
 uint32_t now=g_now+200U; char expected[256]; const char *p_expected_text=expected;
 strcpy(expected,p_expected[step]);
 if(17U==step) memcpy(expected+9,"12345678901234567890",20);
 if(!p_tx[0]) {pump(now); pump(now);}
 assert(p_tx[0] && strlen(p_expected_text)==g_tx_len[0]);
 assert(!memcmp(p_tx[0],p_expected_text,g_tx_len[0]));
 complete(COMM_PORT_4G); pump(g_now+1U);
 assert(step+1U==gm_status().commands_sent);
 assert(step==gm_status().commands_confirmed);
}
static void acknowledge(unsigned step)
{
 reply(0U==step ? "a" : (1U==step ? "+ok\r\n" : "\r\nOK\r\n"));
 assert(step+1U==gm_status().commands_confirmed);
}
static void through(unsigned count)
{
 unsigned step;
 for(step=gm_status().commands_confirmed;step<count;step++)
 {issue(step); acknowledge(step);}
}
static void finish(svc_gm800_state_t expected)
{
 assert(SVC_GM800_FINISHING==gm_status().state);
 flush_logs(); pump(g_now+GM800_LOG_TAIL_MS); flush_logs();
 assert(!svc_gm800_busy() && expected==gm_status().state);
 assert(!g_reset_count);
}
static void parser_test(void)
{
 proto_gm800_response_t parser; uint8_t data[160]; unsigned i;
 proto_gm800_response_reset(&parser);
 assert(!proto_gm800_response_feed(&parser,2,(const uint8_t*)"AT+TOKEN=OK\r\nNOTOK\r\n",20));
 assert(!proto_gm800_response_feed(&parser,2,(const uint8_t*)"O",1));
 assert(1==proto_gm800_response_feed(&parser,2,(const uint8_t*)"K\r\n",3));
 memset(data,'X',sizeof(data));
 assert(!proto_gm800_response_feed(&parser,2,data,sizeof(data)));
 assert(!proto_gm800_response_feed(&parser,2,(const uint8_t*)"OK\r\n",4));
 assert(3==proto_gm800_response_feed(&parser,2,(const uint8_t*)"OK\r\nERROR\r\n",11));
 assert(!proto_gm800_response_feed(NULL,2,data,1));
 assert(!proto_gm800_response_feed(&parser,PROTO_GM800_COMMAND_COUNT,data,1));
 proto_gm800_response_reset(&parser);
 assert(1==proto_gm800_response_feed(&parser,0,(const uint8_t*)"a",1));
 proto_gm800_response_reset(&parser);
 assert(!proto_gm800_response_feed(&parser,1,(const uint8_t*)"OK\r\n",4));
 assert(1==proto_gm800_response_feed(&parser,1,(const uint8_t*)"a+ok\r\n",6));
 for(i=0;i<2;i++) {proto_gm800_response_reset(&parser);}
}
int main(int argc,char **argv)
{
 int scenario; unsigned i; uint32_t before; svc_gm800_status_t status;
 assert(2==argc); scenario=atoi(argv[1]); assert(app_init());
 if(15==scenario){parser_test();return 0;}
 if(16==scenario) g_now=UINT32_MAX-1000U;
 begin(1==scenario ? COMM_PORT_RS485_2 : COMM_PORT_RS485_1);
 switch(scenario)
 {
 case 0: case 1: case 16:
  through(PROTO_GM800_COMMAND_COUNT); finish(SVC_GM800_CONFIRMED);
  assert(1==gm_status().sequences_sent && !gm_status().failures); break;
 case 2:
  through(PROTO_GM800_COMMAND_COUNT);finish(SVC_GM800_CONFIRMED);
  begin(COMM_PORT_RS485_2);through(PROTO_GM800_COMMAND_COUNT);finish(SVC_GM800_CONFIRMED);
  assert(2==gm_status().sessions_started&&2==gm_status().sequences_sent);break;
 case 3:
  through(2);issue(2);reply("ERROR\r\n");finish(SVC_GM800_RESPONSE_ERROR);
  assert(3==gm_status().failed_step&&3==gm_status().commands_sent);
  before=g_capture_count;pump(g_now+10000);assert(before==g_capture_count);
  begin(COMM_PORT_RS485_2);through(PROTO_GM800_COMMAND_COUNT);finish(SVC_GM800_CONFIRMED);break;
 case 4:
  issue(0);pump(g_now+GM800_RESPONSE_TIMEOUT_MS);
  finish(SVC_GM800_RESPONSE_TIMEOUT); assert(1==gm_status().failed_step);
  before=g_capture_count;pump(g_now+10000);assert(before==g_capture_count);break;
 case 5:
  through(2);issue(2);reply("AT+WKMOD=NET\r\nNOTOK\r\nO");
  assert(2==gm_status().commands_confirmed); reply("K\r\n");
  assert(3==gm_status().commands_confirmed);break;
 case 6:
  issue(0);reply("+ERR=1\r\n");finish(SVC_GM800_RESPONSE_ERROR);
  assert(1==gm_status().failed_step);break;
 case 7:
  through(PROTO_GM800_STEP_SAVE_FIRST);issue(PROTO_GM800_STEP_SAVE_FIRST);pump(g_now+2500);assert(svc_gm800_busy());
  assert(PROTO_GM800_STEP_SAVE_FIRST==gm_status().commands_confirmed);acknowledge(PROTO_GM800_STEP_SAVE_FIRST);
  through(PROTO_GM800_COMMAND_COUNT);finish(SVC_GM800_CONFIRMED);break;
 case 8:
  through(PROTO_GM800_STEP_SAVE_FIRST);issue(PROTO_GM800_STEP_SAVE_FIRST);pump(g_now+GM800_SAVE_TIMEOUT_MS);
  finish(SVC_GM800_RESPONSE_TIMEOUT);
  assert(PROTO_GM800_STEP_SAVE_FIRST+1==gm_status().failed_step&&PROTO_GM800_STEP_SAVE_FIRST+1==gm_status().commands_sent);break;
 case 9:
  through(PROTO_GM800_STEP_RESTART);issue(PROTO_GM800_STEP_RESTART);reply("WH-GM800\r\n");
  pump(g_now+GM800_RESPONSE_TIMEOUT_MS);finish(SVC_GM800_RESPONSE_TIMEOUT);
  assert(PROTO_GM800_COMMAND_COUNT==gm_status().failed_step&&!gm_status().sequences_sent);break;
 case 10:
  issue(0);
  {const uint8_t value=0x55;
   for(i=0;i<COMM_SERIAL_QUEUE_DEPTH;i++)
    assert(SVC_SERIAL_OK==svc_serial_send(g_test_port,&value,1));}
  reply("a");assert(1==gm_status().commands_confirmed&&gm_status().log_dropped);
  through(PROTO_GM800_COMMAND_COUNT);finish(SVC_GM800_CONFIRMED);break;
 case 11:
  pump(g_now+GM800_SESSION_TIMEOUT_MS);finish(SVC_GM800_TIMEOUT);
  assert(!gm_status().commands_sent);break;
 case 12:
  g_force_busy=1;pump(g_now+200);pump(g_now+1);pump(g_now+1000);
  g_force_busy=0;finish(SVC_GM800_TRANSPORT_ERROR);
  begin(COMM_PORT_RS485_2);through(PROTO_GM800_COMMAND_COUNT);finish(SVC_GM800_CONFIRMED);break;
 case 13:
  assert(!svc_gm800_start(COMM_PORT_RS485_2,"98765432109876543210",21));
  {const uint8_t save[]={0x14,6,0,0x21,0xDD,0xCD,0x43,0xC0};
   uint32_t now=g_now+60;
   for(i=0;i<sizeof(save);i++) inject(COMM_PORT_RS485_2,save[i],now+i);
   pump(now+i+50);}
  assert(!g_reset_count);
  through(PROTO_GM800_COMMAND_COUNT);finish(SVC_GM800_CONFIRMED);break;
 case 14:
  assert(!svc_gm800_start(COMM_PORT_4G,"12345678901234567890",21));
  assert(!svc_gm800_start(COMM_PORT_NUM,"12345678901234567890",21));
  assert(!svc_gm800_start(COMM_PORT_RS485_1,NULL,21));
  assert(!svc_gm800_start(COMM_PORT_RS485_1,"bad",4));break;
 case 17:
  through(2);issue(2);reply("OK\r\nERROR\r\n");
  finish(SVC_GM800_RESPONSE_ERROR);assert(2==gm_status().commands_confirmed);break;
 case 18:
  reply("OK\r\n");issue(0);pump(g_now+GM800_RESPONSE_TIMEOUT_MS);
  finish(SVC_GM800_RESPONSE_TIMEOUT);break;
 case 19:
  through(PROTO_GM800_STEP_RESTART);issue(PROTO_GM800_STEP_RESTART);reply("\r\nO");assert(PROTO_GM800_STEP_RESTART==gm_status().commands_confirmed);
  reply("K\r\n");finish(SVC_GM800_CONFIRMED);break;
 case 20:
  issue(0);huart1.ErrorCode=1U;HAL_UART_ErrorCallback(&huart1);
  pump(g_now+1);finish(SVC_GM800_TRANSPORT_ERROR);break;
 case 21:
  pump(g_now+200);pump(g_now);assert(p_tx[0]);
  reply("a");assert(!gm_status().commands_confirmed&&p_tx[0]);
  complete(COMM_PORT_4G);pump(g_now+1);assert(1==gm_status().commands_confirmed);break;
 case 22:
  issue(0);acknowledge(0);issue(1);reply("OK\r\n");
  pump(g_now+GM800_RESPONSE_TIMEOUT_MS);finish(SVC_GM800_RESPONSE_TIMEOUT);
  assert(2==gm_status().failed_step);break;

 case 23:
  through(PROTO_GM800_STEP_ICCID);issue(PROTO_GM800_STEP_ICCID);
  reply("AT+ICCID?\r\n+ICCID:12345678901234567890\r\n");
  assert(PROTO_GM800_STEP_ICCID==gm_status().commands_confirmed);
  assert(gm_status().log_frames);
  reply("O");assert(PROTO_GM800_STEP_ICCID==gm_status().commands_confirmed);
  reply("K\r\n");
  issue(PROTO_GM800_STEP_IMEI);
  reply("AT+IMEI?\r\n+IMEI:123456789012345\r\nOK\r\n");
  assert(PROTO_GM800_STEP_SAVE_FIRST==gm_status().commands_confirmed);
  through(PROTO_GM800_COMMAND_COUNT);finish(SVC_GM800_CONFIRMED);break;
 case 24:
  through(PROTO_GM800_STEP_IMEI);issue(PROTO_GM800_STEP_IMEI);
  reply("ERROR\r\n");finish(SVC_GM800_RESPONSE_ERROR);
  assert(PROTO_GM800_STEP_IMEI+1==gm_status().failed_step);
  before=g_capture_count;pump(g_now+10000);assert(before==g_capture_count);break;
 default:abort();
 }
 status=gm_status();assert(status.commands_confirmed<=status.commands_sent);
 return 0;
}
"""

class Gm800Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("gcc")
        if compiler is None:
            raise RuntimeError("Native GCC is required")
        cls.temp = tempfile.TemporaryDirectory(prefix="comm-convert-gm800-")
        cls.addClassCleanup(cls.temp.cleanup)
        directory = pathlib.Path(cls.temp.name)
        (directory / "usart.h").write_text(HAL_HEADER,encoding="utf-8")
        # Approved profile fixture: independent of the current command table.
        # The migrated repository no longer contains Program/.
        values = [
            '+++',
            'a',
            'AT+WKMOD=NET\r\n',
            'AT+SOCKAEN=ON\r\n',
            'AT+SOCKA=TCP,62195.cnsh.iot-tcp.com,15000\r\n',
            'AT+SOCKASL=LONG\r\n',
            'AT+SHORTATM=10\r\n',
            'AT+SOCKRSTIM=10\r\n',
            'AT+SOCKRSNUM=60\r\n',
            'AT+HEARTEN=ON\r\n',
            'AT+HEARTTM=30\r\n',
            'AT+HEARTTP=NET\r\n',
            'AT+HEARTSORT=USER\r\n',
            'AT+HEARTDT=7777772E7573722E636E\r\n',
            'AT+REGEN=ON\r\n',
            'AT+REGSND=LINK\r\n',
            'AT+REGTP=CLOUD\r\n',
            None,
            'AT+ICCID?\r\n',
            'AT+IMEI?\r\n',
            'AT+S\r\n',
            'AT+S\r\n',
            'AT+Z\r\n',
        ]
        encoded = [json.dumps(value) if value is not None else
                   '"AT+CLOUD=00000000000000000000" GM800_CLOUD_AUTH_SUFFIX'
                   for value in values]
        expected = '#include "gm800_config.h"\n' + \
            "static const char *const p_expected[] = {\n" + \
            ",\n".join(encoded) + "\n};\n"
        source = HARNESS.replace("int main(int argc, char **argv)",
                                 "int serial_base_main(int argc, char **argv)")
        source = source.replace("g_tx_log[32]","g_tx_log[128]")
        source = source.replace(
            "static uint8_t g_tx_log[128];",
            """static uint8_t g_tx_log[128];
static uint8_t g_capture[128][256];
static uint16_t g_capture_length[128];
static uint32_t g_capture_time[128];
static unsigned g_capture_count;""")
        source = source.replace(
            "g_tx_log[g_tx_calls++] = p_buffer[0U];",
            """g_tx_log[g_tx_calls++] = p_buffer[0U];
    assert(g_capture_count < 128U && length <= 256U);
    memcpy(g_capture[g_capture_count],p_buffer,length);
    g_capture_length[g_capture_count]=length;
    g_capture_time[g_capture_count]=g_now;
    g_capture_count++;""")
        source += DTO_HARNESS.replace("int main(int argc, char **argv)",
                                     "int dto_base_main(int argc, char **argv)")
        harness=directory/"gm800_harness.c"
        harness.write_text(source+expected+GM_HARNESS,encoding="utf-8")
        cls.executable=directory/"gm800_test.exe"
        command=[compiler, '-DCOMM_APP_ENABLE_MODE=0',"-DCOMM_APP_ENABLE_DTO=1","-DCOMM_APP_ENABLE_HIK=0","-std=c99","-Wall","-Wextra","-Werror"]
        for include in (directory,ROOT/"Config",ROOT/"Bsp",ROOT/"Services",
                        ROOT/"App",ROOT/"Protocols"):
            command+=["-I",str(include)]
        for path in ("Bsp/bsp_uart.c","Services/svc_serial.c",
                     "Protocols/proto_gm800.c","Services/svc_gm800.c",
                     "App/app_dto.c","App/app_main.c",
                     "Services/svc_identity.c", "Services/svc_relay.c", "Services/svc_telemetry.c", "App/app_local.c","Protocols/proto_local.c",
                     "Services/svc_params.c", "Services/svc_boot.c","Config/comm_params.c"):
            command.append(str(ROOT/path))
        command += [str(harness),"-o",str(cls.executable)]
        subprocess.run(command,check=True,cwd=ROOT,timeout=60)

    def test_configuration(self):
        scenarios = ['confirmed legacy profile RS485_1', 'confirmed legacy profile RS485_2', 'external repeated configuration', 'ERROR stops, no auto retry, external restart', 'handshake timeout no auto retry', 'echo ignored, fragmented OK accepted', 'handshake ERROR stops', 'save response longer than normal timeout', 'save timeout prevents later save and reboot', 'boot banner cannot acknowledge restart', 'log congestion cannot suppress ACK verification', 'bounded session timeout', 'transport failure exits without reset and external retry', 'busy local save/reconfiguration discarded', 'start API guards', 'stream parser bounds and false OK protection', 'tick rollover confirmed sequence', 'ERROR wins over coalesced OK', 'stale unsolicited OK cannot advance next command', 'fragmented final OK and finishing drain', 'RX error invalidates acknowledgement', 'early ACK waits for UART TC', 'wrong handshake response rejected', 'ICCID and IMEI reply data log with split OK before saves', 'IMEI failure stops before save and never auto retries']
        for index,name in enumerate(scenarios):
            with self.subTest(scenario=name):
                subprocess.run([str(self.executable),str(index)],check=True,
                               timeout=10,cwd=ROOT)
                print("PASS:",name)

if __name__ == "__main__":
    unittest.main(verbosity=2)
