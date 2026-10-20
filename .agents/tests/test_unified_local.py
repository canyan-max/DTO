"""Unified 17..34 register map on every UART in DTO and acquisition modes."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

from test_serial import HAL_HEADER, HARNESS, ROOT
from test_dto import DTO_HARNESS
from test_poll import POLL_HARNESS
from test_mode import MODE_HARNESS

UNIFIED_HARNESS = r'''
static void local_request(comm_port_t port,uint8_t function,uint16_t reg,
                          uint16_t value,uint32_t time)
{
 uint8_t p[]={0x14,0,0,0,0,0,0,0};
 p[1]=function;p[2]=(uint8_t)(reg>>8);p[3]=(uint8_t)reg;
 p[4]=(uint8_t)(value>>8);p[5]=(uint8_t)value;crc(p,6);
 input(port,p,8,time);assert(p_tx[port]);
 assert(0x14==p_tx[port][0]);
}
static void finish_local(comm_port_t port,uint32_t time)
{complete(port);tick(time);}
static void only_source(comm_port_t port)
{unsigned i;for(i=0;i<3;i++)if(i!=(unsigned)port)assert(!p_tx[i]);}
int main(int argc,char **argv)
{
 unsigned n=argc>1?(unsigned)atoi(argv[1]):0;
 unsigned mode=0;comm_port_t port=COMM_PORT_4G;
 test_nv_reset();if(n>=12&&n<18)g_relay_nv=1;
 assert(app_init());
 if(n<18){mode=(n%6)/3;port=(comm_port_t)(n%3);}
 if(n<18&&mode)change(1,100);
 if(n<6)
 {
  local_request(port,3,17,18,1100);only_source(port);
  assert(41==g_tx_len[port]&&36==p_tx[port][2]);
  assert(0x27==p_tx[port][21]&&0x10==p_tx[port][22]);
  assert(0x2E==p_tx[port][23]&&0xE0==p_tx[port][24]);
  assert(0x25==p_tx[port][29]&&0x80==p_tx[port][30]);
  assert(0==p_tx[port][35]&&0==p_tx[port][36]&&mode==p_tx[port][38]);
  assert(!memcmp(p_tx[port]+11,mode?"HIK_V2.0.0":"DTO_V2.0.0",10));
  assert(proto_local_crc16(p_tx[port],39)==
         ((uint16_t)p_tx[port][39]|((uint16_t)p_tx[port][40]<<8)));
  finish_local(port,1200);
  local_request(port,6,26,10500,1250);only_source(port);
  assert(settings().uv_protect_mv==10500&&!g_param_nv_writes);
  finish_local(port,1350);
  local_request(port,5,17,0xFF00,1400);only_source(port);
  assert(g_relay_gpio[0]&&!g_relay_nv_writes);
  finish_local(port,1500);assert(g_relay_nv_writes==1&&g_relay_nv==1&&!g_reset_count);
  local_request(port,1,17,4,1600);only_source(port);
  assert(9==g_tx_len[port]&&4==p_tx[port][2]);
  assert(p_tx[port][3]==1&&p_tx[port][4]==0&&p_tx[port][5]==0&&p_tx[port][6]==1);
  assert(!g_param_nv_writes&&mode==(unsigned)g_mode.active);
 }
 else if(n<12)
 {
  comm_params_t p=settings();p.uv_protect_mv=10500;assert(svc_params_set(&p));
  local_request(port,6,33,56781,1100);only_source(port);
  assert(!g_param_nv_writes&&!g_reset_count);
  finish_local(port,1200);
  assert(g_param_nv_writes==1&&g_reset_count==1);
  assert((uint16_t)g_nv_data[1]==10500&&((g_nv_data[1]>>40)&255)==mode);
  svc_params_init();assert(settings().app_mode==mode&&settings().uv_protect_mv==10500);
 }
 else if(n<18)
 {
  comm_params_t p=settings();p.uv_protect_mv=10500;assert(svc_params_set(&p));
  local_request(port,6,33,52445,1100);only_source(port);
  assert(!g_param_nv_writes&&!g_reset_count);
  finish_local(port,1200);
  assert(g_reset_count==1&&g_param_nv_writes==1);
  assert(settings().uv_protect_mv==10000&&!settings().app_mode);
  assert(g_relay_nv==1&&!g_relay_nv_writes);
 }
 else switch(n)
 {
 case 18:
 {
  unsigned i;uint8_t p[]={0x14,6,0,26,0x29,4,0,0};
  for(i=0;i<3;i++)
  {
   input((comm_port_t)i,p,8,100+i*300);
   assert(!p_tx[0]&&!p_tx[1]&&!p_tx[2]&&settings().uv_protect_mv==10000);
  }
  break;
 }
 case 19:
 {
  proto_local_snapshot_t s={0};proto_local_command_t c;
  uint8_t p[]={0x14,3,0,17,0,18,0,0};uint8_t out[42];uint16_t length;
  s.params=settings();crc(p,6);memset(out,0xA5,sizeof(out));
  assert(PROTO_LOCAL_DROP==proto_local_process(p,8,&s,&c,out,40,&length));
  assert(!length&&out[40]==0xA5&&out[41]==0xA5);
  assert(PROTO_LOCAL_REPLY==proto_local_process(p,8,&s,&c,out,41,&length));
  assert(length==41&&out[41]==0xA5);
  assert(PROTO_LOCAL_DROP==proto_local_process(p,8,NULL,&c,out,41,&length));
  assert(PROTO_LOCAL_DROP==proto_local_process(p,8,&s,NULL,out,41,&length));
  p[5]=28;crc(p,6);
  assert(PROTO_LOCAL_REPLY==proto_local_process(p,8,&s,&c,out,41,&length));
  assert(length==5&&out[1]==0x83&&out[2]==2);
  assert(APP_LOCAL_ERROR==app_local_accept(COMM_PORT_NUM,p,8));break;
 }
 case 20:
 case 21:
 {
  uint8_t p[3][8]={{0x14,3,0,26,0,1,0,0},
                   {0x14,3,0,21,0,1,0,0},{0x14,3,0,17,0,1,0,0}};
  unsigned order[3]={1,2,0};unsigned i,j;
  if(n==21){change(1,100);order[0]=0;order[1]=2;order[2]=1;}
  for(i=0;i<3;i++)crc(p[i],6);
  for(j=0;j<8;j++)for(i=0;i<3;i++)inject((comm_port_t)i,p[i][j],1100+j);
  tick(1157);
  for(i=0;i<3;i++)
  {
   port=(comm_port_t)order[i];assert(p_tx[port]);only_source(port);
   assert(g_tx_len[port]==7);
   if(port==COMM_PORT_4G)assert(p_tx[port][3]==0x27&&p_tx[port][4]==0x10);
   else if(port==COMM_PORT_RS485_1)assert(p_tx[port][3]==(n==21?'H':'D')&&p_tx[port][4]==(n==21?'I':'T'));
   else assert(!p_tx[port][3]&&!p_tx[port][4]);
   finish_local(port,1200+i*100);
  }
  assert(!g_param_nv_writes&&!g_relay_nv_writes);break;
 }
 case 22:
 {
  uint8_t p[]={0x14,3,0,26,0,1,0,0};uint16_t fixed[14]={100};
  uint8_t reply[40];uint16_t length;unsigned i;
  change(1,100);start_query(2000,0,0);
  length=response(reply,3,fixed,14);
  for(i=0;i<length;i++)inject(COMM_PORT_RS485_2,reply[i],2020+i);
  crc(p,6);for(i=0;i<8;i++)inject(COMM_PORT_RS485_2,p[i],2120+i);
  tick(2177);dto_step();
  assert(1==g_poll.received&&p_tx[2]&&g_tx_len[2]==7);
  assert(p_tx[2][0]==0x14&&p_tx[2][3]==0x27&&p_tx[2][4]==0x10);break;
 }
 case 23:
 {
  uint8_t p[]={0x14,6,0,33,0xDD,0xCD,0,0};crc(p,6);g_force_busy=1;
  input(COMM_PORT_RS485_1,p,8,100);tick(1400);tick(1500);
  assert(!g_param_nv_writes&&!g_reset_count);break;
 }
 case 24:
 {
  uint8_t p[]={0x14,0x10,0,29,0,4,8,0,0,0x4B,0,0,0,0x96,0,0,0};crc(p,15);
  input(COMM_PORT_RS485_2,p,17,100);assert(p_tx[2]&&g_tx_len[2]==8);only_source(COMM_PORT_RS485_2);
  assert(settings().rs485_1_baud==19200&&settings().rs485_2_baud==38400);
  assert(huart2.Init.BaudRate==9600&&huart3.Init.BaudRate==9600&&!g_param_nv_writes);break;
 }
 case 25:
 {
  uint8_t p[]={0x15,5,0,17,0xFF,0,0,0};crc(p,6);
  input(COMM_PORT_RS485_1,p,8,100);
  assert(p_tx[0]&&!memcmp(p_tx[0],p,8)&&!g_relay_gpio[0]);
  assert(!p_tx[1]&&!p_tx[2]);break;
 }
 default:abort();
 }
 return 0;
}
'''


class UnifiedLocalTests(unittest.TestCase):
    def test_unified_local(self):
        compiler = shutil.which('gcc')
        if not compiler:
            raise RuntimeError('Native GCC required')
        names = [f'{action}, {mode}, UART{port}'
                 for action in ['full map/params/relay/legacy01', 'explicit save', 'restore defaults']
                 for mode in ['DTO','HIK'] for port in [1,2,3]]
        names += ['bad CRC ignored on every port', 'full-span output and null guards',
                  'concurrent three-port requests keep DTO reply sources',
                  'concurrent three-port requests keep HIK reply sources',
                  'controller reply preserves next queued local command',
                  'failed RS485 save ACK cannot write or reset',
                  'paired baud write via UART3 remains RAM until save/reboot',
                  'other slave addresses retain DTO forwarding']
        with tempfile.TemporaryDirectory(prefix='comm-convert-unified-') as tmp:
            directory=pathlib.Path(tmp)
            (directory/'usart.h').write_text(HAL_HEADER,encoding='utf-8')
            source=HARNESS.replace('int main(int argc, char **argv)',
                                    'int serial_base_main(int argc, char **argv)')
            source=source.replace('static uint32_t g_reset_count;',
                                  'static uint32_t g_reset_count;\nstatic unsigned g_param_nv_writes,g_mode_flash_fail;')
            source=source.replace('memcpy(g_nv_data, p_words, 24U);',
                                  'g_param_nv_writes++;if(g_mode_flash_fail)return 0U;\nmemcpy(g_nv_data, p_words, 24U);')
            source+=DTO_HARNESS.replace('int main(int argc, char **argv)',
                                        'int dto_base_main(int argc, char **argv)')
            source+=POLL_HARNESS.replace('int main(int argc,char **argv)',
                                         'int poll_base_main(int argc,char **argv)')
            source+=MODE_HARNESS.replace('int main(int argc,char **argv)',
                                         'int mode_base_main(int argc,char **argv)')
            harness=directory/'harness.c'
            harness.write_text(source+UNIFIED_HARNESS,encoding='utf-8')
            executable=directory/'unified.exe'
            command=[compiler,'-std=c99','-Wall','-Wextra','-Werror']
            for include in (directory,*(ROOT/p for p in ['Config','Bsp','Services','App','Protocols'])):
                command+=['-I',str(include)]
            paths=['Bsp/bsp_uart.c','Services/svc_serial.c','Protocols/proto_gm800.c',
                   'Services/svc_gm800.c','Services/svc_params.c','Services/svc_boot.c','Config/comm_params.c',
                   'App/app_dto.c','App/app_main.c','App/app_local.c','App/app_mode.c',
                   'Protocols/proto_local.c','Services/svc_identity.c','Services/svc_relay.c',
                   'App/app_hik.c','Protocols/proto_hik.c','Services/svc_telemetry.c',
                   'Services/svc_poll.c','Protocols/proto_controller.c']
            command += [str(ROOT/p) for p in paths]+[str(harness),'-o',str(executable)]
            subprocess.run(command,check=True,timeout=60)
            for index,name in enumerate(names):
                with self.subTest(scenario=name):
                    subprocess.run([str(executable),str(index)],check=True,timeout=10)
                    print('PASS:',name)


if __name__=='__main__':
    unittest.main(verbosity=2)
