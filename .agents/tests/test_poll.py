"""Actual UART/App polling integration with simulated EVE/SZS controllers."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

from test_serial import HAL_HEADER, HARNESS, ROOT
from test_dto import DTO_HARNESS

POLL_HARNESS = r'''
#include "svc_poll.h"
#include "svc_telemetry.h"
#include "svc_identity.h"
#include "svc_gm800.h"
#include "proto_gm800.h"
#include "proto_local.h"
#include "proto_hik.h"
#include "poll_config.h"
static void tick(uint32_t time)
{g_now=time;dto_step();dto_step();dto_step();}
static void crc(uint8_t *p,uint16_t n)
{uint16_t c=proto_local_crc16(p,n);p[n]=(uint8_t)c;p[n+1]=(uint8_t)(c>>8);}
static void input(comm_port_t port,const uint8_t *p,uint16_t n,uint32_t time)
{
 unsigned i;for(i=0;i<n;i++)inject(port,p[i],time+i);
 tick(time+n-1+(port==COMM_PORT_4G?15:50));
}
static svc_telemetry_snapshot_t snapshot(void)
{svc_telemetry_snapshot_t s;assert(svc_telemetry_get(&s));return s;}
static uint16_t response(uint8_t *p,uint8_t function,const uint16_t *words,unsigned n)
{
 unsigned i;p[0]=1;p[1]=function;p[2]=(uint8_t)(2*n);
 for(i=0;i<n;i++){p[3+2*i]=(uint8_t)(words[i]>>8);p[4+2*i]=(uint8_t)words[i];}
 crc(p,(uint16_t)(3+2*n));return (uint16_t)(5+2*n);
}
static void start_query(uint32_t time,unsigned profile,unsigned step)
{
 uint8_t request[8];
 tick(time);
 assert(profile==(unsigned)g_poll.profile&&step==g_poll.step);
 assert(p_tx[2]&&8==g_tx_len[2]);
 assert(8==proto_controller_query(g_poll.profile,g_poll.step,request,8));
 assert(!memcmp(request,p_tx[2],8));
 complete(COMM_PORT_RS485_2);tick(time+10);
 assert(SVC_POLL_WAITING==g_poll.state);
}
static void answer(uint8_t function,const uint16_t *words,unsigned n,uint32_t time)
{uint8_t p[40];uint16_t len=response(p,function,words,n);input(COMM_PORT_RS485_2,p,len,time);}
static uint16_t g_test_capacity = 100;
static uint16_t g_test_soc = 67;
static void eve(uint32_t base)
{
 uint16_t fixed[14]={100},solar[]={1800,123},temp[]={2500},soc[]={67};
 uint16_t state[]={0,12},battery[]={2600,0xFF9C,0xFFFF};
 fixed[0]=g_test_capacity;soc[0]=g_test_soc;
 start_query(base+1000,0,0);answer(3,fixed,14,base+1020);
 assert(!snapshot().battery_valid);
 start_query(base+2000,0,1);answer(4,solar,2,base+2020);
 assert(snapshot().solar_valid&&!snapshot().battery_valid);
 start_query(base+3000,0,2);answer(4,temp,1,base+3020);
 start_query(base+4000,0,3);answer(4,soc,1,base+4020);
 start_query(base+5000,0,4);answer(4,state,2,base+5020);
 assert(!snapshot().battery_valid);
 start_query(base+6000,0,5);answer(4,battery,3,base+6020);
}
static void hik(uint8_t command,uint32_t time)
{
 uint8_t p[]={0xDD,0xA5,0,0,0xFF,0,0x77};p[2]=command;p[5]=(uint8_t)(0-command);
 input(COMM_PORT_RS485_1,p,7,time);assert(p_tx[1]);
}
static void codec_guards(void)
{
 uint16_t words[]={2600,0xFF9C,0xFFFF};uint8_t p[40];unsigned i;
 comm_battery_data_t b={0},before={0};comm_solar_data_t s={0};
 uint16_t n=response(p,4,words,3);
 assert(PROTO_CONTROLLER_DATA==proto_controller_decode(0,5,p,n,&b,&s));
 assert(-100==b.current_10ma);
 before=b;
 for(i=0;i<n;i++)
 {p[i]^=1;assert(PROTO_CONTROLLER_INVALID==proto_controller_decode(0,5,p,n,&b,&s));assert(!memcmp(&b,&before,sizeof(b)));p[i]^=1;}
 for(i=0;i<n;i++)assert(PROTO_CONTROLLER_INVALID==proto_controller_decode(0,5,p,i,&b,&s));
 assert(PROTO_CONTROLLER_INVALID==proto_controller_decode(0,5,NULL,n,&b,&s));
 assert(PROTO_CONTROLLER_INVALID==proto_controller_decode(0,5,p,n,NULL,&s));
 assert(PROTO_CONTROLLER_INVALID==proto_controller_decode(0,5,p,n,&b,NULL));
 assert(PROTO_CONTROLLER_INVALID==proto_controller_decode(2,5,p,n,&b,&s));
 assert(PROTO_CONTROLLER_INVALID==proto_controller_decode(0,6,p,n,&b,&s));
 assert(PROTO_CONTROLLER_INVALID==proto_controller_decode(0,4,p,n,&b,&s));
 p[0]=2;crc(p,n-2);assert(PROTO_CONTROLLER_INVALID==proto_controller_decode(0,5,p,n,&b,&s));
 p[0]=1;p[1]=3;crc(p,n-2);assert(PROTO_CONTROLLER_INVALID==proto_controller_decode(0,5,p,n,&b,&s));
 assert(!proto_controller_query(0,0,p,7));assert(!proto_controller_query(0,0,NULL,8));
 assert(!proto_controller_query(2,0,p,8));
}
int main(int argc,char **argv)
{
 unsigned n=argc>1?(unsigned)atoi(argv[1]):0;
 test_nv_reset();if(9==n)g_now=UINT32_MAX-500;
 assert(app_init());
 switch(n)
 {
 case 0:
 {
  static const uint16_t addresses[]={0x9001,0x3100,0x3110,0x311A,0x3200,0x331A,0xE002,0x0100,0x00FD};
  static const uint8_t counts[]={14,2,1,1,2,3,10,9,1};
  unsigned i;uint8_t p[8];
  for(i=0;i<9;i++)
  {
   assert(8==proto_controller_query(i<6?0:1,i<6?i:i-6,p,8));
   assert(1==p[0]&&(i>0&&i<6?4:3)==p[1]);
   assert(addresses[i]==(((uint16_t)p[2]<<8)|p[3]));assert(counts[i]==p[5]&&!p[4]);
   assert(proto_local_crc16(p,6)==((uint16_t)p[6]|((uint16_t)p[7]<<8)));
  }
  break;
 }
 case 1:
 {
  svc_telemetry_snapshot_t s;uint8_t out[40];uint16_t len;
  eve(0);s=snapshot();assert(s.battery_valid&&s.solar_valid);
  assert(2600==s.battery.voltage_10mv&&-100==s.battery.current_10ma);
  assert(10000==s.battery.nominal_10mah&&67==s.battery.soc_percent);
  assert(6700==s.battery.remaining_10mah);
  assert(1==s.battery.ntc_count&&2981==s.battery.temperature_deci_kelvin[0]);
  assert(1800==s.solar.voltage_10mv&&123==s.solar.current_10ma);
  hik(3,6200);len=proto_hik_build_battery(&s.battery,out,sizeof(out));
  assert(len==g_tx_len[1]&&!memcmp(out,p_tx[1],len));
  complete(COMM_PORT_RS485_1);tick(6300);
  hik(15,6400);len=proto_hik_build_solar(&s.solar,out,sizeof(out));
  assert(len==g_tx_len[1]&&!memcmp(out,p_tx[1],len));
  assert(6==g_poll.received&&1==g_poll.battery_updates&&1==g_poll.solar_updates);
  break;
 }
 case 2:
 {
  uint16_t fixed[10]={80},live[]={90,255,0,0xFB19,0,150,0,180,125},state[]={3};
  tick(5000);assert(1==g_poll.profile&&1==g_poll.switches);
  start_query(5600,1,0);answer(3,fixed,10,5620);
  start_query(6600,1,1);answer(3,live,9,6620);
  assert(snapshot().battery_valid&&snapshot().solar_valid);
  assert(2550==snapshot().battery.voltage_10mv&&-150==snapshot().battery.current_10ma);
  assert(8000==snapshot().battery.nominal_10mah&&90==snapshot().battery.soc_percent);
  assert(7200==snapshot().battery.remaining_10mah);
  assert(2981==snapshot().battery.temperature_deci_kelvin[0]);
  assert(2681==snapshot().battery.temperature_deci_kelvin[1]);
  start_query(7600,1,2);answer(3,state,1,7620);
  assert(1==g_poll.step&&0==snapshot().battery.protection);
  live[2]=250;start_query(8600,1,1);answer(3,live,9,8620);
  assert(250==snapshot().battery.current_10ma);
  break;
 }
 case 3:codec_guards();break;
 case 4:
 {
  uint8_t p[]={1,0x83,2,0,0};start_query(1000,0,0);crc(p,3);
  input(COMM_PORT_RS485_2,p,5,1020);
  assert(1==g_poll.exceptions&&!g_poll.received&&1==g_poll.step);
  assert(!snapshot().battery_valid);
  start_query(2000,0,1);break;
 }
 case 5:
 {
  uint32_t t;for(t=0;t<=12000;t+=10)
  {tick(t);if(p_tx[2]){complete(COMM_PORT_RS485_2);dto_step();}}
  assert(g_poll.timeouts>=4&&g_poll.switches>=2&&0==g_poll.profile);
  assert(!snapshot().battery_valid&&!snapshot().solar_valid);
  assert(g_feed_count>1000);break;
 }
 case 6:
 {
  uint16_t words[14]={100};uint8_t p[40];uint16_t len;
  start_query(1000,0,0);len=response(p,3,words,14);p[4]^=1;
  input(COMM_PORT_RS485_2,p,len,1020);assert(1==g_poll.invalid&&0==g_poll.step);
  p[4]^=1;input(COMM_PORT_RS485_2,p,len,1150);
  assert(1==g_poll.received&&1==g_poll.step);
  break;
 }
 case 7:
 {
  const uint8_t config[]="AT+CLOUD=12345678901234567890";
  uint8_t sn[16]={0};svc_identity_status_t identity;
  sn[15]='A';assert(svc_identity_set_sn(sn,16));
  start_query(1000,0,0);input(COMM_PORT_RS485_1,config,sizeof(config)-1,1100);
  assert(svc_gm800_busy()&&SVC_POLL_PAUSED==g_poll.state);
  tick(2000);assert(1==g_poll.sent);
  assert(svc_identity_get_status(&identity)&&identity.sn[15]=='A'&&16==identity.length);
  assert(!g_relay_nv_writes&&!g_reset_count);
  break;
 }
 case 8:
  g_force_busy=1;tick(1000);assert(SVC_POLL_QUEUED==g_poll.state);
  tick(2010);tick(2020);assert(g_poll.tx_failures||g_poll.timeouts);break;
 case 9:
 {
  uint32_t base=UINT32_MAX-500;
  eve(base);assert(snapshot().battery_valid&&6==g_poll.received);
  break;
 }
 case 10:
 {
  uint16_t fixed[14]={100};uint8_t p[40];uint16_t len;
  start_query(1000,0,0);tick(1610);assert(1==g_poll.timeouts);
  len=response(p,3,fixed,14);input(COMM_PORT_RS485_2,p,len,1650);
  assert(!g_poll.received&&!snapshot().battery_valid);
  tick(2200);assert(!p_tx[2]);tick(2400);assert(p_tx[2]);
  break;
 }
 case 11:
  hik(3,100);assert(0==p_tx[1][4]&&0==p_tx[1][5]);
  assert(!p_tx[2]&&!snapshot().battery_valid);break;
 case 12:
 {
  uint8_t p[40];uint16_t fixed[14]={100};uint16_t len;
  g_force_busy=1;tick(1000);len=response(p,3,fixed,14);
  input(COMM_PORT_RS485_2,p,len,1020);
  assert(!g_poll.received&&g_poll.invalid);break;
 }
 case 13:
 {
  uint8_t p[40];comm_battery_data_t b={0};comm_solar_data_t s={0};
  uint16_t a[]={100,0x8000,0};uint16_t len=response(p,4,a,3);
  assert(PROTO_CONTROLLER_INVALID==proto_controller_decode(0,5,p,len,&b,&s));
  a[1]=0;a[2]=0x8000;len=response(p,4,a,3);
  assert(PROTO_CONTROLLER_INVALID==proto_controller_decode(0,5,p,len,&b,&s));
  a[1]=0x8000;a[2]=0xFFFF;len=response(p,4,a,3);
  assert(PROTO_CONTROLLER_DATA==proto_controller_decode(0,5,p,len,&b,&s));
  assert(-32768==b.current_10ma);
  a[1]=0x7FFF;a[2]=0;len=response(p,4,a,3);
  assert(PROTO_CONTROLLER_DATA==proto_controller_decode(0,5,p,len,&b,&s));
  assert(32767==b.current_10ma);
  a[0]=101;len=response(p,4,a,1);
  assert(PROTO_CONTROLLER_INVALID==proto_controller_decode(0,3,p,len,&b,&s));
  {uint16_t fixed[10]={656};len=response(p,3,fixed,10);}
  assert(PROTO_CONTROLLER_INVALID==proto_controller_decode(1,0,p,len,&b,&s));
  {uint16_t live[9]={90,6554};len=response(p,3,live,9);}
  assert(PROTO_CONTROLLER_INVALID==proto_controller_decode(1,1,p,len,&b,&s));
  a[0]=(uint16_t)-505;len=response(p,4,a,1);
  assert(PROTO_CONTROLLER_DATA==proto_controller_decode(0,2,p,len,&b,&s));
  assert(2680==b.temperature_deci_kelvin[0]);
  a[0]=(uint16_t)-27311;len=response(p,4,a,1);
  assert(PROTO_CONTROLLER_INVALID==proto_controller_decode(0,2,p,len,&b,&s));
  break;
 }
 case 14:
 {
  uint16_t fixed[14]={100};uint8_t p[40];uint16_t len;
  start_query(1000,0,0);answer(3,fixed,14,1020);
  tick(1800);inject(COMM_PORT_RS485_2,0x55,1990);tick(1990);tick(2000);
  assert(!p_tx[2]);tick(2040);assert(p_tx[2]);
  complete(COMM_PORT_RS485_2);tick(2050);
  len=response(p,4,fixed,2);input(COMM_PORT_RS485_2,p,len,2070);
  assert(2==g_poll.received);break;
 }
 case 15:
 {
  uint16_t solar[]={1800,10};
  eve(0);
  start_query(7000,0,1);answer(4,solar,2,7020);
  start_query(8000,0,2);tick(8610);assert(g_poll.timeouts);
  start_query(9300,0,3);tick(9910);
  start_query(10600,0,4);tick(11210);
  assert(snapshot().battery_valid);
  start_query(11900,0,5);tick(12510);
  tick(13110);assert(1==g_poll.profile&&!snapshot().solar_valid);
  break;
 }
 case 16:
 {
  uint8_t sn[16]={0};svc_identity_status_t id;
  sn[15]='Z';assert(svc_identity_set_sn(sn,16));
  svc_poll_pause();assert(SVC_POLL_PAUSED==g_poll.state);
  tick(2000);assert(SVC_POLL_QUIET==g_poll.state&&!p_tx[2]);
  start_query(3000,0,0);
  assert(svc_identity_get_status(&id)&&id.sn[15]=='Z');
  break;
 }
 case 17:
 {
  const uint8_t config[]="AT+CLOUD=12345678901234567890";
  uint32_t t;svc_gm800_status_t status;
  start_query(1000,0,0);input(COMM_PORT_RS485_2,config,sizeof(config)-1,1100);
  assert(SVC_POLL_PAUSED==g_poll.state);
  for(t=1200;t<7500;t+=10)
  {
   tick(t);
   if(p_tx[0])
   {
    unsigned k;const char *response;svc_gm800_status_t progress;
    assert(svc_gm800_get_status(&progress));
    response=0==progress.commands_confirmed ? "a" :
             (1==progress.commands_confirmed ? "+ok\r\n" : "OK\r\n");
    complete(COMM_PORT_4G);dto_step();
    for(k=0;k<strlen(response);k++)inject(COMM_PORT_4G,response[k],t);
   }
   if(p_tx[2]){complete(COMM_PORT_RS485_2);dto_step();}
  }
  assert(svc_gm800_get_status(&status));
  assert(SVC_GM800_CONFIRMED==status.state&&PROTO_GM800_COMMAND_COUNT==status.commands_confirmed);
  assert(g_poll.sent>1&&SVC_POLL_PAUSED!=g_poll.state&&!g_poll.switches);
  break;
 }
 case 18:
 {
  uint16_t solar[]={1800,10},soc[]={66},state[]={0,0},battery[]={2500,10,0};
  eve(0);
  start_query(7000,0,1);answer(4,solar,2,7020);
  start_query(8000,0,2);tick(8610);
  start_query(9300,0,3);answer(4,soc,1,9320);
  start_query(10300,0,4);answer(4,state,2,10320);
  start_query(11300,0,5);answer(4,battery,3,11320);
  assert(snapshot().battery_valid);
  start_query(12300,0,1);answer(4,solar,2,12320);
  tick(13300);
  assert(snapshot().solar_valid&&!snapshot().battery_valid&&!g_poll.switches);
  assert(1==g_poll.battery_updates);break;
 }
 case 19:
 {
  uint8_t p[]={0x14,5,0,17,0xFF,0,0,0};
  start_query(1000,0,0);crc(p,6);input(COMM_PORT_4G,p,8,1100);
  assert(p_tx[0]&&SVC_POLL_PAUSED==g_poll.state&&g_relay_gpio[0]);
  complete(COMM_PORT_4G);tick(1200);tick(1300);
  assert(g_relay_nv_writes==1&&g_relay_nv==1&&!g_reset_count);
  tick(2400);assert(p_tx[2]&&SVC_POLL_PAUSED!=g_poll.state);
  assert(g_relay_gpio[0]);break;
 }
 case 20:
 {
  uint16_t solar[]={1430,208};
  eve(0);
  /* Five EVE live queries take at least five seconds. A normal 100ms
   * scheduling/response variation must not turn the previous solar reply zero. */
  start_query(7100,0,1);
  hik(15,7115);
  assert(0x07==p_tx[1][5]&&0x08==p_tx[1][6]);
  complete(COMM_PORT_RS485_1);tick(7180);
  answer(4,solar,2,7200);
  hik(15,7350);
  assert(0x05==p_tx[1][5]&&0x96==p_tx[1][6]);
  break;
 }

 case 21:
 {
  uint8_t request[]={0x14,3,0,39,0,1,0,0};unsigned port;
  g_test_capacity=200;g_test_soc=88;eve(0);
  assert(17600==snapshot().battery.remaining_10mah);
  hik(3,6200);assert(0x44==p_tx[1][8]&&0xC0==p_tx[1][9]);
  complete(COMM_PORT_RS485_1);tick(6300);crc(request,6);
  for(port=0;port<3;port++)
  {
   input((comm_port_t)port,request,8,6400+port*120);
   assert(p_tx[port]&&0x44==p_tx[port][3]&&0xC0==p_tx[port][4]);
   complete((comm_port_t)port);tick(g_now+1);
  }
  break;
 }
 case 22: case 23: case 24: case 25:
 {
  if(22==n){g_test_capacity=655;g_test_soc=100;}
  if(23==n){g_test_capacity=200;g_test_soc=0;}
  if(24==n){g_test_capacity=0;g_test_soc=88;}
  if(25==n){g_test_capacity=1;g_test_soc=33;}
  eve(0);
  assert((22==n?65500:(25==n?33:0))==snapshot().battery.remaining_10mah);
  svc_telemetry_invalidate();hik(3,6200);
  assert(!p_tx[1][8]&&!p_tx[1][9]);break;
 }
 default:abort();
 }
 return 0;
}
'''


class PollTests(unittest.TestCase):
    def test_poll(self):
        compiler = shutil.which('gcc')
        if not compiler:
            raise RuntimeError('Native GCC required')
        names = ['legacy query addresses/counts and independent CRC',
                 'EVE full acquisition through UART and HIK reply',
                 'SZS detection, acquisition, signed temperatures and current',
                 'codec CRC/address/function/length/null guards',
                 'exception advances without valid cache',
                 'timeouts and repeated profile detection without blocking',
                 'bad reply followed by valid retry in same transaction',
                 'GM800 pauses polling and preserves shared RAM SN',
                 'hardware TX busy failure bounded', '32-bit clock wrap',
                 'late response rejected and quiet quarantine',
                 'host still receives zero before polling succeeds',
                 'unsent queued request cannot accept measurements',
                 'signed current boundaries and range validation',
                 'new partial RX postpones next query until quiet',
                 'disconnect expires measurements and detects another profile',
                 'maintenance resume starts a new transaction, preserves SN',
                 'complete GM800 session and polling restart without overlap',
                 'fresh solar/SOC cannot refresh missing old battery temperature',
                 'shared relay automatic save pauses and resumes polling',
                 'normal EVE cycle jitter does not produce a zero solar reply',
                 '200 Ah at 88 percent is 176 Ah on HIK and all three local ports',
                 'maximum controller capacity without multiplication overflow',
                 'zero SOC estimates zero remaining charge',
                 'missing capacity estimates zero remaining charge',
                 'fractional Ah calculation and invalid cache still returns zero']
        with tempfile.TemporaryDirectory(prefix='comm-convert-poll-') as tmp:
            directory = pathlib.Path(tmp)
            (directory/'usart.h').write_text(HAL_HEADER, encoding='utf-8')
            source = HARNESS.replace('int main(int argc, char **argv)',
                                     'int serial_base_main(int argc, char **argv)')
            source += DTO_HARNESS.replace('int main(int argc, char **argv)',
                                          'int dto_base_main(int argc, char **argv)')
            harness = directory/'harness.c'
            harness.write_text(source.replace('g_tx_log[32]', 'g_tx_log[128]')+POLL_HARNESS, encoding='utf-8')
            executable = directory/'poll.exe'
            command = [compiler, '-DCOMM_APP_ENABLE_MODE=0', '-std=c99', '-Wall', '-Wextra', '-Werror']
            for include in (directory, *(ROOT/p for p in
                                         ['Config','Bsp','Services','App','Protocols'])):
                command += ['-I', str(include)]
            paths = ['Bsp/bsp_uart.c','Services/svc_serial.c','Protocols/proto_gm800.c',
                     'Services/svc_gm800.c','Services/svc_params.c','Services/svc_boot.c','Config/comm_params.c',
                     'App/app_dto.c','App/app_main.c','App/app_local.c',
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
