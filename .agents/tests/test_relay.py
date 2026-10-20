"""DTO relay/protection integration: actual App, serial, relay, fake HAL/NV/ADC."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

from test_serial import HAL_HEADER, HARNESS, ROOT
from test_dto import DTO_HARNESS
from test_local import LOCAL_HARNESS

RELAY_HARNESS = r'''
#include "svc_relay.h"
#include "svc_gm800.h"
#include "proto_local.h"
#include "dto_config.h"
static svc_relay_status_t relay_status(void)
{svc_relay_status_t s;assert(svc_relay_get_status(&s));return s;}
static void control(uint16_t address,uint16_t value,uint32_t base)
{
 uint8_t request[]={0x14U,5U,0U,0U,0U,0U,0U,0U};
 request[3U]=(uint8_t)address;request[4U]=(uint8_t)(value>>8U);
 request[5U]=(uint8_t)value;send_request(request,8U,base);
}
static void uv_enable(uint8_t enable)
{
 comm_params_t p=params();p.uv_enabled=enable;assert(svc_params_set(&p));
}
static void voltage(uint16_t mv,uint8_t valid,uint32_t elapsed)
{
 uint32_t left=elapsed;
 g_test_sampling.input_mv=mv;g_test_sampling.valid=valid;
 assert(svc_relay_task());
 while(left)
 {
  uint32_t step=left>100U?100U:left;
  g_now+=step;left-=step;assert(svc_relay_task());
 }
}
static void reset_relay_gpio(void)
{g_relay_gpio[0U]=0U;g_relay_gpio[1U]=0U;assert(svc_relay_init());}
int main(int argc,char **argv)
{
 int n=argc>1?atoi(argv[1]):0;
 if(n==1)g_relay_nv=13U; /* Old 4-bit record; only channels 1/2 restored. */
 if(n==2)g_relay_nv=0U;
 if(n==14)g_relay_nv=0x100U;
 test_nv_reset();assert(app_init());
 switch(n)
 {
 case 0:
  assert(!g_relay_gpio[0U]&&!g_relay_gpio[1U]);
  assert(!relay_status().from_flash&&!g_relay_nv_writes);
  assert(!svc_relay_set(2U,1U)&&!svc_relay_set(3U,1U));break;
 case 1:
  assert(g_relay_gpio[0U]&&!g_relay_gpio[1U]&&relay_status().from_flash);
  assert(!g_relay_nv_writes);break;
 case 2:
  assert(!g_relay_gpio[0U]&&!g_relay_gpio[1U]&&relay_status().from_flash);
  control(17U,0U,100U);finish_reply();assert(!g_relay_nv_writes);break;
 case 3:
  control(17U,0xFF00U,100U);
  assert(g_relay_gpio[0U]&&!g_relay_gpio[1U]);
  assert(!g_relay_nv_writes&&p_tx[0U]&&p_tx[0U][1U]==5U);
  finish_reply();assert(g_relay_nv==1U&&g_relay_nv_writes==1U);
  assert(!g_reset_count&&p_rx[0U]&&p_rx[1U]&&p_rx[2U]);
  assert(APP_LOCAL_DONE==local_status().state);
  control(18U,0xFF00U,300U);finish_reply();assert(g_relay_nv==3U);
  reset_relay_gpio();assert(g_relay_gpio[0U]&&g_relay_gpio[1U]);break;
 case 4:
  control(17U,0xFF00U,100U);finish_reply();
  control(17U,0xFF00U,300U);finish_reply();assert(g_relay_nv_writes==1U);
  control(17U,0U,500U);finish_reply();assert(g_relay_nv==0U);
  reset_relay_gpio();assert(!g_relay_gpio[0U]&&!g_relay_gpio[1U]);break;
 case 5:
  control(19U,0xFF00U,100U);
  assert(p_tx[0U][1U]==0x85U&&p_tx[0U][2U]==2U);finish_reply();
  control(20U,0U,300U);assert(p_tx[0U][2U]==2U);finish_reply();
  control(17U,1U,500U);assert(p_tx[0U][2U]==3U);finish_reply();
  assert(!g_relay_nv_writes&&!g_relay_gpio[0U]);break;
 case 6:
  {uint8_t read[]={0x14U,3U,0U,17U,0U,17U,0U,0U};
   send_request(read,8U,100U);assert(g_tx_len[0U]==39U);
   assert(reg16(p_tx[0U]+3U)==0U&&reg16(p_tx[0U]+7U)==1U);
   assert(!memcmp(p_tx[0U]+11U,DTO_FIRMWARE_VERSION,10U));
   assert(reg16(p_tx[0U]+21U)==10000U);
   assert(proto_local_crc16(p_tx[0U],37U)==
          ((uint16_t)p_tx[0U][38U]<<8U|p_tx[0U][37U]));finish_reply();
   read[1U]=1U;read[5U]=4U;send_request(read,8U,300U);
   assert(g_tx_len[0U]==9U&&p_tx[0U][2U]==4U);
   assert(!memcmp(p_tx[0U]+3U,"\0\0\1\1",4U));finish_reply();}
  break;
 case 7:
  g_relay_nv_fail=1U;control(17U,0xFF00U,100U);finish_reply();
  assert(g_relay_nv_writes==1U&&relay_status().failures==1U);
  assert(relay_status().dirty&&p_rx[0U]&&APP_LOCAL_FAILED==local_status().state);
  dto_step();dto_step();assert(g_relay_nv_writes==1U&&!g_reset_count);
  g_relay_nv_fail=0U;control(17U,0xFF00U,300U);finish_reply();
  assert(g_relay_nv_writes==2U&&g_relay_nv==1U);break;
 case 8:
  control(17U,0xFF00U,100U);g_now+=200U;dto_step();
  assert(APP_LOCAL_FAILED==local_status().state&&!g_relay_nv_writes);
  assert(relay_status().dirty);break;
 case 9:
  assert(svc_relay_set(0U,1U));assert(svc_relay_set(1U,1U));uv_enable(1U);
  voltage(10000U,1U,2999U);assert(!relay_status().uv_active);
  voltage(10000U,1U,1U);assert(relay_status().uv_active);
  assert(!g_relay_gpio[0U]&&!g_relay_gpio[1U]);
  assert(relay_status().requested[0U]&&relay_status().requested[1U]);
  voltage(12000U,1U,4000U);assert(relay_status().uv_active);
  voltage(12001U,1U,2999U);assert(relay_status().uv_active);
  voltage(12001U,1U,1U);assert(!relay_status().uv_active);
  assert(g_relay_gpio[0U]&&g_relay_gpio[1U]&&!g_relay_nv_writes);break;
 case 10:
  uv_enable(1U);voltage(9000U,1U,2000U);voltage(11000U,1U,100U);
  voltage(9000U,1U,2999U);assert(!relay_status().uv_active);
  voltage(9000U,1U,1U);assert(relay_status().uv_active);
  voltage(13000U,1U,2000U);voltage(12000U,1U,100U);
  voltage(13000U,1U,2999U);assert(relay_status().uv_active);
  voltage(13000U,1U,1U);assert(!relay_status().uv_active);break;
 case 11:
  uv_enable(1U);voltage(9000U,0U,4000U);assert(!relay_status().uv_active);
  voltage(9000U,1U,3000U);assert(relay_status().uv_active);
  voltage(13000U,0U,5000U);assert(relay_status().uv_active);
  uv_enable(0U);assert(svc_relay_task());assert(!relay_status().uv_active);break;
 case 12:
  assert(svc_relay_set(0U,1U));uv_enable(1U);
  voltage(9000U,1U,3000U);assert(relay_status().uv_active);
  control(18U,0xFF00U,g_now+100U);finish_reply();
  assert(!g_relay_gpio[0U]&&!g_relay_gpio[1U]&&g_relay_nv==3U);
  control(17U,0U,g_now+100U);finish_reply();assert(g_relay_nv==2U);
  voltage(13000U,1U,3000U);
  assert(!g_relay_gpio[0U]&&g_relay_gpio[1U]);break;
 case 13:
  uv_enable(1U);g_now=0xFFFFFF00U;
  voltage(9000U,1U,3000U);assert(relay_status().uv_active);
  uv_enable(0U);voltage(9000U,1U,1U);
  uv_enable(1U);voltage(9000U,1U,2000U);g_now+=5000U;
  voltage(9000U,1U,0U);assert(!relay_status().uv_active);
  voltage(9000U,1U,3000U);assert(relay_status().uv_active);break;
 case 14:
  assert(!relay_status().from_flash&&!g_relay_gpio[0U]);break;
 case 15:
  {uint8_t packet[8U]={0x14U,5U,0U,17U,0xFFU,0U,0U,0U};
   unsigned i;for(i=0;i<8;i++)inject(COMM_PORT_4G,packet[i],100U+i);
   g_now=122U;dto_step();dto_step();assert(!g_relay_gpio[0U]);
   assert(!p_tx[0U]&&!p_tx[1U]&&!p_tx[2U]);}
  break;
 case 16:
  g_now=1000U;dto_step();assert(g_led_gpio[0U]);
  inject(COMM_PORT_RS485_1,0x55U,1001U);dto_step();assert(g_led_gpio[1U]);
  g_now=1101U;dto_step();assert(!g_led_gpio[1U]);
  uv_enable(1U);voltage(9000U,1U,3000U);dto_step();
  assert(!g_led_gpio[0U]&&!g_led_gpio[1U]&&!g_led_gpio[2U]);break;
 case 17:
  {proto_local_snapshot_t snapshot={0};uint8_t reply[41U];uint16_t len=99U;
   uint8_t packet[]={0x14U,5U,0U,17U,0xFFU,0U,0U,0U};proto_local_command_t cmd;
   snapshot.params=params();crc_frame(packet,6U);
   assert(PROTO_LOCAL_DROP==proto_local_process(packet,8U,NULL,&cmd,reply,41U,&len));
   assert(len==0U);
   assert(PROTO_LOCAL_DROP==proto_local_process(packet,8U,&snapshot,&cmd,reply,7U,&len));
   assert(PROTO_LOCAL_DROP==proto_local_process(packet,7U,&snapshot,&cmd,reply,41U,&len));
   assert(!svc_relay_get_status(NULL));}
  break;
 case 18:
  {uint8_t old[200U]={0xAAU};
   assert(SVC_SERIAL_OK==svc_serial_send(COMM_PORT_RS485_1,old,sizeof(old)));
   assert(SVC_SERIAL_OK==svc_serial_send(COMM_PORT_RS485_2,old,sizeof(old)));
   svc_serial_task();control(17U,0xFF00U,100U);finish_reply();
   assert(!g_relay_nv_writes&&APP_LOCAL_WAIT_TX==local_status().state);
   complete(COMM_PORT_RS485_1);dto_step();assert(!g_relay_nv_writes);
   complete(COMM_PORT_RS485_2);dto_step();assert(g_relay_nv_writes==1U);
   assert(p_rx[0U]&&p_rx[1U]&&p_rx[2U]);}
  break;
 case 19:
  {uint8_t packet[]={0x14U,5U,0U,17U,0xFFU,0U,0U,0U};
   assert(svc_gm800_start(COMM_PORT_RS485_1, "01234567890123456789",21U));
   assert(APP_LOCAL_RETRY==app_local_accept(COMM_PORT_4G,packet,8U));
   assert(!svc_relay_save()&&!g_relay_nv_writes&&!g_relay_gpio[0U]);}
  break;
 default:assert(0);
 }
 return 0;
}
'''


class RelayTests(unittest.TestCase):
    def test_relay(self):
        compiler=shutil.which('gcc')
        if compiler is None:
            raise RuntimeError('Native GCC required')
        names=['low default and removed channel guards','legacy record mask',
               'zero persisted state','ACK then save without reset and reload',
               'same request skips erase and low reload','removed/invalid writes',
               'combined registers, version, inverse aliases and legacy 01',
               'Flash failure no automatic retry','failed ACK prevents save',
               'UV delays, threshold boundaries and request restore',
               'continuous dwell reset','invalid samples and disable UV',
               'commands during protection persist only requested state',
               'clock rollover and stalled scheduler','corrupt legacy record',
               'bad CRC no action or forwarding','LED heartbeat/activity/UV',
               'codec boundaries','relay save waits for all three TX queues',
               'GM800 configuration excludes relay maintenance']
        with tempfile.TemporaryDirectory(prefix='comm-convert-relay-') as tmp:
            directory=pathlib.Path(tmp)
            (directory/'usart.h').write_text(HAL_HEADER,encoding='utf-8')
            source=HARNESS.replace('int main(int argc, char **argv)',
                                   'int serial_base_main(int argc, char **argv)')
            source+=DTO_HARNESS.replace('int main(int argc, char **argv)',
                                        'int dto_base_main(int argc, char **argv)')
            # Reuse only the request helpers, not the parameter-only scenarios.
            source+=LOCAL_HARNESS.split('static void load_saved(void)')[0]
            harness=directory/'harness.c'
            harness.write_text(source+RELAY_HARNESS,encoding='utf-8')
            executable=directory/'relay.exe'
            command=[compiler, '-DCOMM_APP_ENABLE_MODE=0',"-DCOMM_APP_ENABLE_DTO=1","-DCOMM_APP_ENABLE_HIK=0",'-std=c99','-Wall','-Wextra','-Werror']
            for include in (directory,ROOT/'Config',ROOT/'Bsp',ROOT/'Services',
                            ROOT/'App',ROOT/'Protocols'):
                command+=['-I',str(include)]
            paths=['Bsp/bsp_uart.c','Services/svc_serial.c','Protocols/proto_gm800.c',
                   'Services/svc_gm800.c','Services/svc_params.c','Services/svc_boot.c','Config/comm_params.c',
                   'App/app_dto.c','App/app_main.c','Services/svc_telemetry.c', 'App/app_local.c',
                   'Protocols/proto_local.c','Services/svc_identity.c','Services/svc_relay.c']
            command += [str(ROOT/p) for p in paths]+[str(harness),'-o',str(executable)]
            subprocess.run(command,check=True,timeout=60)
            for index,name in enumerate(names):
                with self.subTest(scenario=name):
                    subprocess.run([str(executable),str(index)],check=True,timeout=10)
                    print('PASS:',name)


if __name__=='__main__':
    unittest.main(verbosity=2)
