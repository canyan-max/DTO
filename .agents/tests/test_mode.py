"""Runtime mode ownership, common services and explicit single-page persistence."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

from test_serial import HAL_HEADER, HARNESS, ROOT
from test_dto import DTO_HARNESS
from test_poll import POLL_HARNESS

MODE_HARNESS = r'''
#include "app_mode.h"
#include "svc_params.h"
#include "svc_relay.h"
#include "app_local.h"
#include "params_config.h"
static comm_params_t settings(void)
{comm_params_t p;assert(svc_params_get(&p));return p;}
static void command(uint8_t function,uint16_t reg,uint16_t value,uint32_t time)
{
 uint8_t p[]={0x14,0,0,0,0,0,0,0};
 p[1]=function;p[2]=(uint8_t)(reg>>8);p[3]=(uint8_t)reg;
 p[4]=(uint8_t)(value>>8);p[5]=(uint8_t)value;crc(p,6);
 input(COMM_PORT_4G,p,8,time);assert(p_tx[0]);
}
static void change(unsigned mode,uint32_t time)
{
 unsigned previous=g_mode.active;
 command(6,34,(uint16_t)mode,time);
 assert(6==p_tx[0][1]&&mode==p_tx[0][5]);
 assert(previous==(unsigned)g_mode.active);
 assert(previous==settings().app_mode);
 complete(COMM_PORT_4G);tick(time+100);
 if(mode!=previous)assert(APP_MODE_QUIET==g_mode.state);
 tick(time+900);
 assert(mode==(unsigned)g_mode.active&&mode==settings().app_mode);
 assert(APP_MODE_ACTIVE==g_mode.state&&!g_reset_count);
}
static void record(unsigned mode)
{
 uint32_t sum;
 g_nv_data[0]=(uint64_t)19200|((uint64_t)38400<<32);
 g_nv_data[1]=(uint64_t)10500|((uint64_t)12500<<16)|((uint64_t)mode<<40);
 sum=(uint32_t)g_nv_data[0]^(uint32_t)(g_nv_data[0]>>32)^
     (uint32_t)g_nv_data[1]^(uint32_t)(g_nv_data[1]>>32);
 g_nv_data[2]=PARAMS_VALID_MAGIC|((uint64_t)sum<<32);
}
static void port_command(comm_port_t port,uint8_t function,uint16_t value,uint32_t time)
{
 uint8_t p[]={0x14,0,0,34,0,0,0,0};
 p[1]=function;p[4]=(uint8_t)(value>>8);p[5]=(uint8_t)value;crc(p,6);
 input(port,p,8,time);assert(p_tx[port]);
 assert(!p_tx[0]);
}
int main(int argc,char **argv)
{
 unsigned n=argc>1?(unsigned)atoi(argv[1]):0;
 test_nv_reset();
 if(1==n)record(0);
 if(5==n)record(1);
 if(13==n)g_now=UINT32_MAX-500;
 if(18==n)record(2);
 assert(app_init());
 switch(n)
 {
 case 0:
  assert(COMM_APP_MODE_DTO==g_mode.active&&!settings().app_mode);
  tick(10000);assert(!p_tx[0]&&!p_tx[1]&&!p_tx[2]);
  {uint8_t p[]={1,2,0,4};input(COMM_PORT_4G,p,4,10100);
   assert(p_tx[1]&&p_tx[2]&&!memcmp(p_tx[1],p,4)&&!memcmp(p_tx[2],p,4));}
  assert(!g_param_nv_writes);break;
 case 1:
  assert(!g_mode.active&&!settings().app_mode);
  assert(19200==huart2.Init.BaudRate&&38400==huart3.Init.BaudRate);
  assert(10500==settings().uv_protect_mv&&!g_param_nv_writes);break;
 case 2:
  change(1,100);assert(1==g_mode.switches&&!g_param_nv_writes);
  assert(!p_tx[2]);start_query(2000,0,0);break;
 case 3:
  change(1,100);hik(3,1100);assert(30==g_tx_len[1]&&!p_tx[1][4]);
  complete(COMM_PORT_RS485_1);tick(1200);change(0,1300);
  {uint8_t p[]={0x21,0,0xFF};input(COMM_PORT_4G,p,3,2400);
   assert(p_tx[1]&&p_tx[2]&&!memcmp(p_tx[2],p,3));}
  assert(2==g_mode.switches&&!g_param_nv_writes);break;
 case 4:
  change(1,100);command(6,33,56781,1100);
  assert(!g_param_nv_writes&&!g_reset_count);
  complete(COMM_PORT_4G);tick(1200);
  assert(1==g_param_nv_writes&&1==g_reset_count);
  assert(1==((g_nv_data[1]>>40)&255));
  svc_params_init();assert(1==settings().app_mode);break;
 case 5:
  assert(COMM_APP_MODE_HIK==g_mode.active&&1==settings().app_mode);
  assert(19200==huart2.Init.BaudRate);start_query(1000,0,0);break;
 case 6:
  change(1,100);assert(!g_param_nv_writes);
  svc_params_init();assert(!settings().app_mode);
  assert(app_mode_init((comm_app_mode_t)settings().app_mode));
  assert(COMM_APP_MODE_DTO==g_mode.active);break;
 case 7:
  command(6,34,2,100);assert(0x86==p_tx[0][1]&&3==p_tx[0][2]);
  complete(COMM_PORT_4G);tick(200);assert(!g_mode.switches&&!settings().app_mode);
  {uint8_t p[]={0x14,6,0,34,0,1,0,0};input(COMM_PORT_4G,p,8,300);}
  assert(!p_tx[0]&&!g_mode.switches&&!g_param_nv_writes);break;
 case 8:
  g_force_busy=1;
  {uint8_t p[]={0x14,6,0,34,0,1,0,0};crc(p,6);input(COMM_PORT_4G,p,8,100);}
  tick(1300);tick(1400);assert(!g_mode.switches&&!settings().app_mode);
  assert(!g_param_nv_writes&&!g_reset_count);break;
 case 9:
 {
  uint8_t p[256]={0x42};
  assert(SVC_SERIAL_OK==svc_serial_send(COMM_PORT_RS485_1,p,sizeof(p)));
  assert(SVC_SERIAL_OK==svc_serial_send(COMM_PORT_RS485_2,p,sizeof(p)));
  tick(10);command(6,34,1,100);complete(COMM_PORT_4G);tick(200);
  assert(!g_mode.active&&APP_MODE_ACTIVE==g_mode.state);
  complete(COMM_PORT_RS485_1);tick(210);assert(!g_mode.active);
  complete(COMM_PORT_RS485_2);tick(220);assert(APP_MODE_QUIET==g_mode.state);
  tick(919);assert(!g_mode.active);tick(920);assert(1==g_mode.active);
  break;
 }
 case 10:
  command(6,34,1,100);complete(COMM_PORT_4G);tick(200);
  inject(COMM_PORT_RS485_2,0xEE,600);tick(600);
  tick(1000);assert(!g_mode.active&&!p_tx[0]);
  tick(1300);assert(1==g_mode.active&&!p_tx[0]&&!p_tx[2]);break;
 case 11:
 {
  uint8_t sn[16]={0};svc_identity_status_t identity;svc_relay_status_t relay;
  sn[15]='A';assert(svc_identity_set_sn(sn,16));assert(svc_relay_set(0,1));
  change(1,100);change(0,1100);change(1,2100);
  assert(svc_identity_get_status(&identity)&&identity.sn[15]=='A'&&identity.writes==1);
  assert(svc_relay_get_status(&relay)&&relay.requested[0]&&relay.output[0]);
  assert(g_relay_gpio[0]&&!g_relay_nv_writes&&!g_param_nv_writes);
  break;
 }
 case 12:
 {
  uint32_t t;assert(app_mode_request(COMM_APP_MODE_HIK));tick(0);
  for(t=100;t<=20100;t+=100){inject(COMM_PORT_RS485_2,0x55,t);tick(t);}
  assert(1==g_mode.failures&&!g_mode.active&&!g_mode.switches);
  tick(20800);assert(APP_MODE_ACTIVE==g_mode.state&&!g_mode.active);
  assert(!settings().app_mode&&!g_param_nv_writes);break;
 }
 case 13:
  change(1,UINT32_MAX-400);assert(1==g_mode.switches);break;
 case 14:
 {
  uint8_t p[256]={0x42};unsigned i;
  change(1,100);
  for(i=0;i<4;i++)assert(SVC_SERIAL_OK==svc_serial_send(COMM_PORT_RS485_1,p,sizeof(p)));
  hik(3,1100);command(6,34,0,1200);complete(COMM_PORT_4G);tick(1300);
  for(i=0;i<4;i++){complete(COMM_PORT_RS485_1);tick(1310+i*10);}
  tick(2040);assert(!g_mode.active&&APP_MODE_ACTIVE==g_mode.state);
  assert(!p_tx[0]&&!p_tx[1]&&!p_tx[2]);break;
 }
 case 15:
 {
  const uint8_t config[]="AT+CLOUD=12345678901234567890";
  input(COMM_PORT_RS485_1,config,sizeof(config)-1,100);
  assert(svc_gm800_busy()&&!app_mode_request(COMM_APP_MODE_HIK));
  assert(!g_mode.switches&&!settings().app_mode);break;
 }
 case 16:
  command(3,34,1,100);assert(7==g_tx_len[0]&&!p_tx[0][4]);
  complete(COMM_PORT_4G);tick(200);change(1,300);
  command(3,34,1,1300);assert(1==p_tx[0][4]);
  assert(!g_param_nv_writes);break;
 case 17:
  change(1,100);g_mode_flash_fail=1;command(6,33,56781,1100);
  complete(COMM_PORT_4G);tick(1200);tick(1300);
  assert(1==g_mode.active&&1==settings().app_mode&&!g_reset_count);
  assert(UINT64_MAX==g_nv_data[1]);break;
 case 18:
  assert(!g_mode.active&&!settings().app_mode&&9600==huart2.Init.BaudRate);break;
 case 19:
  change(1,100);command(6,33,52445,1100);complete(COMM_PORT_4G);tick(1200);
  assert(1==g_reset_count&&!settings().app_mode);
  svc_params_init();assert(!settings().app_mode);break;
 case 20:
  change(1,100);eve(1000);assert(snapshot().battery_valid);
  change(0,7500);assert(!snapshot().battery_valid&&!snapshot().solar_valid);
  change(1,8500);assert(!snapshot().battery_valid);
  assert(!g_poll.received&&!g_param_nv_writes);break;
 case 21:
 {
  svc_relay_status_t relay;comm_params_t params=settings();uint32_t t;
  params.uv_enabled=1;assert(svc_params_set(&params));
  assert(svc_relay_set(0,1));g_test_sampling.valid=1;g_test_sampling.input_mv=9000;
  for(t=10;t<=3010;t+=100)tick(t);
  assert(svc_relay_get_status(&relay)&&relay.uv_active);
  change(1,3100);change(0,4100);
  assert(svc_relay_get_status(&relay)&&relay.uv_active&&relay.requested[0]&&!relay.output[0]);
  assert(1==relay.protection_entries&&!g_relay_gpio[0]);break;
 }
 case 22:
  command(6,34,0,100);complete(COMM_PORT_4G);tick(200);
  assert(!g_mode.switches&&APP_MODE_ACTIVE==g_mode.state&&!g_param_nv_writes);break;
 case 23:
  change(1,100);eve(1000);assert(snapshot().battery_valid);
  command(6,34,1,7500);assert(snapshot().battery_valid&&6==g_poll.received);
  complete(COMM_PORT_4G);tick(7600);
  assert(1==g_mode.switches&&APP_MODE_ACTIVE==g_mode.state&&snapshot().battery_valid);
  command(3,34,1,7700);assert(snapshot().battery_valid&&6==g_poll.received);
  break;
 case 24:
  port_command(COMM_PORT_RS485_1,3,1,100);
  assert(7==g_tx_len[1]&&0==p_tx[1][4]&&!p_tx[2]);
  complete(COMM_PORT_RS485_1);tick(200);
  port_command(COMM_PORT_RS485_2,3,1,300);
  assert(7==g_tx_len[2]&&0==p_tx[2][4]&&!p_tx[1]);break;
 case 25:
 case 26:
 {
  comm_port_t port=25==n?COMM_PORT_RS485_1:COMM_PORT_RS485_2;
  port_command(port,6,1,100);assert(6==p_tx[port][1]&&1==p_tx[port][5]);
  assert(!g_mode.active);complete(port);tick(200);tick(1000);assert(1==g_mode.active);
  port_command(port,6,0,1100);assert(!p_tx[port][5]);
  assert(1==g_mode.active);complete(port);tick(1200);tick(2000);
  assert(!g_mode.active&&2==g_mode.switches&&!g_param_nv_writes);break;
 }
 case 27:
  change(1,100);
  port_command(COMM_PORT_RS485_1,3,1,1100);assert(1==p_tx[1][4]);
  complete(COMM_PORT_RS485_1);tick(1200);
  port_command(COMM_PORT_RS485_2,3,1,1300);assert(1==p_tx[2][4]);
  assert(1==g_mode.switches);break;
 case 28:
 {
  uint8_t p[]={0x14,3,0,34,0,1,0x26,0xC4};
  input(COMM_PORT_RS485_1,p,8,100);assert(!p_tx[0]&&!p_tx[1]&&!p_tx[2]);
  port_command(COMM_PORT_RS485_2,6,2,300);
  assert(0x86==p_tx[2][1]&&3==p_tx[2][2]&&!g_mode.switches);break;
 }
 case 29:
 {
  uint8_t p[]={0x14,5,0,17,0xFF,0,0,0};crc(p,6);
  input(COMM_PORT_RS485_1,p,8,100);
  assert(p_tx[1]&&!memcmp(p_tx[1],p,8)&&g_relay_gpio[0]);
  assert(!p_tx[0]&&!p_tx[2]&&!g_mode.switches);
  complete(COMM_PORT_RS485_1);tick(200);assert(1==g_relay_nv_writes);break;
 }
 case 30:
 {
  uint8_t p[]={0x14,6,0,34,0,1,0,0};crc(p,6);g_force_busy=1;
  input(COMM_PORT_RS485_2,p,8,100);tick(1400);tick(1500);
  assert(!g_mode.active&&!settings().app_mode&&!g_mode.switches);
  assert(!g_param_nv_writes&&!g_reset_count);break;
 }
 case 31:
 {
  uint8_t data[256]={0x42};uint8_t p[]={0x14,6,0,34,0,1,0,0};unsigned i;
  for(i=0;i<4;i++)assert(SVC_SERIAL_OK==svc_serial_send(COMM_PORT_RS485_1,data,256));
  tick(10);crc(p,6);input(COMM_PORT_RS485_1,p,8,100);
  assert(!g_mode.active&&!p_tx[0]);
  for(i=0;i<4;i++){complete(COMM_PORT_RS485_1);tick(200+i*10);assert(!g_mode.active);}
  assert(8==g_tx_len[1]&&!memcmp(p,p_tx[1],8));
  complete(COMM_PORT_RS485_1);tick(240);assert(APP_MODE_QUIET==g_mode.state);
  tick(940);assert(1==g_mode.active&&!g_param_nv_writes);break;
 }
 case 32:
 {
  uint16_t solar[]={1430,208};uint8_t old[16];uint16_t size;
  change(1,100);eve(1000);assert(snapshot().battery_valid);
  start_query(8000,0,1);
  port_command(COMM_PORT_RS485_1,6,0,8020);
  assert(6==p_tx[1][1]&&!p_tx[1][5]&&1==g_mode.active);
  complete(COMM_PORT_RS485_1);tick(8100);
  size=response(old,4,solar,2);input(COMM_PORT_RS485_2,old,size,8150);
  tick(9000);assert(!g_mode.active&&APP_MODE_ACTIVE==g_mode.state);
  assert(!p_tx[0]&&!p_tx[1]&&!p_tx[2]);break;
 }
 case 33:
  change(1,100);eve(1000);hik(3,7300);
  complete(COMM_PORT_RS485_1);tick(7400);
  port_command(COMM_PORT_RS485_1,6,0,7500);
  complete(COMM_PORT_RS485_1);tick(7600);tick(8400);
  assert(!g_mode.active&&2==g_mode.switches);break;
 default:abort();
 }
 return 0;
}
'''


class ModeTests(unittest.TestCase):
    def test_mode(self):
        compiler = shutil.which('gcc')
        if not compiler:
            raise RuntimeError('Native GCC required')
        names = ['default DTO broadcasts without polling', 'old record boots DTO unchanged',
                 'UART1 ACK then quiet then HIK polling', 'HIK host then DTO forwarding',
                 '56781 explicitly saves HIK and requests reset', 'saved HIK boots directly',
                 'unsaved mode returns to DTO after reload', 'invalid value/CRC cannot switch',
                 'failed ACK cannot switch or save', 'all three old TX queues drain first',
                 'late RX resets quiet and never leaks', 'repeat transitions preserve SN/relay',
                 'continuous noise cancels target and safely resumes old mode', 'clock wrap',
                 'pending HIK reply does not leak into DTO', 'GM800 excludes mode switch',
                 'read current mode in both owners', 'Flash failure preserves running mode',
                 'invalid stored mode falls back to DTO', 'restore defaults restores DTO',
                 'real EVE acquisition then mode switch clears stale cache',
                 'active UV protection and relay request survive switches',
                 'same mode request is idempotent',
                 'repeated HIK selection and mode read preserve live cache',
                 'user exact mode read replies on both RS485 ports in DTO',
                 'RS485_1 ACK and bidirectional switching',
                 'RS485_2 ACK and bidirectional switching',
                 'mode read on both RS485 ports in HIK',
                 'RS485 bad CRC/value never switches or forwards',
                 'RS485 relay command is handled locally on the source port',
                 'failed RS485 ACK prevents switching',
                 'congested RS485 reply waits for its own ACK completion',
                 'RS485_1 leaves running EVE polling despite late device reply',
                 'RS485_1 leaves HIK after actual nonzero host reply']
        with tempfile.TemporaryDirectory(prefix='comm-convert-mode-') as tmp:
            directory = pathlib.Path(tmp)
            (directory/'usart.h').write_text(HAL_HEADER, encoding='utf-8')
            source = HARNESS.replace('int main(int argc, char **argv)',
                                     'int serial_base_main(int argc, char **argv)')
            source = source.replace('static uint32_t g_reset_count;',
                                    'static uint32_t g_reset_count;\nstatic unsigned g_param_nv_writes,g_mode_flash_fail;')
            source = source.replace('memcpy(g_nv_data, p_words, 24U);',
                                    'g_param_nv_writes++;if(g_mode_flash_fail)return 0U;\nmemcpy(g_nv_data, p_words, 24U);')
            source += DTO_HARNESS.replace('int main(int argc, char **argv)',
                                          'int dto_base_main(int argc, char **argv)')
            source += POLL_HARNESS.replace('int main(int argc,char **argv)',
                                           'int poll_base_main(int argc,char **argv)')
            harness = directory/'harness.c'
            harness.write_text(source+MODE_HARNESS, encoding='utf-8')
            executable = directory/'mode.exe'
            command = [compiler, '-std=c99', '-Wall', '-Wextra', '-Werror']
            for include in (directory, *(ROOT/p for p in
                                         ['Config','Bsp','Services','App','Protocols'])):
                command += ['-I', str(include)]
            paths = ['Bsp/bsp_uart.c','Services/svc_serial.c','Protocols/proto_gm800.c',
                     'Services/svc_gm800.c','Services/svc_params.c','Services/svc_boot.c','Config/comm_params.c',
                     'App/app_dto.c','App/app_main.c','App/app_local.c','App/app_mode.c',
                     'Protocols/proto_local.c','Services/svc_identity.c','Services/svc_relay.c',
                     'App/app_hik.c','Protocols/proto_hik.c','Services/svc_telemetry.c',
                     'Services/svc_poll.c','Protocols/proto_controller.c']
            command += [str(ROOT/p) for p in paths] + [str(harness), '-o', str(executable)]
            subprocess.run(command, check=True, timeout=60)
            for index, name in enumerate(names):
                with self.subTest(scenario=name):
                    subprocess.run([str(executable),str(index)], check=True, timeout=10)
                    print('PASS:',name)


if __name__ == '__main__':
    unittest.main(verbosity=2)
