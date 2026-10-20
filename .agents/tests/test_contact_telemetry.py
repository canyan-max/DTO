"""Contact polarity, explicit persistence and read-only local telemetry."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

from test_serial import HAL_HEADER, HARNESS, ROOT
from test_dto import DTO_HARNESS
from test_poll import POLL_HARNESS
from test_mode import MODE_HARNESS
from test_unified_local import UNIFIED_HARNESS

EXTENDED_HARNESS = r'''
#include "hik_config.h"
static svc_relay_status_t relay_state(void)
{svc_relay_status_t s;assert(svc_relay_get_status(&s));return s;}
static uint16_t read_word(const uint8_t *p)
{return ((uint16_t)p[0]<<8)|p[1];}
static void publish_values(void)
{
 comm_battery_data_t b={0};comm_solar_data_t s={0};
 b.voltage_10mv=1310;b.current_10ma=-41;b.soc_percent=100;
 b.remaining_10mah=4200;b.nominal_10mah=10000;
 b.ntc_count=1;b.temperature_deci_kelvin[0]=2676;
 s.number=1;s.voltage_10mv=1430;s.current_10ma=208;
 assert(svc_telemetry_publish_battery(&b)&&svc_telemetry_publish_solar(&s));
}
static void record_checksum(void)
{
 uint32_t sum=(uint32_t)g_nv_data[0]^(uint32_t)(g_nv_data[0]>>32)^
              (uint32_t)g_nv_data[1]^(uint32_t)(g_nv_data[1]>>32);
 g_nv_data[2]=PARAMS_VALID_MAGIC|((uint64_t)sum<<32);
}
int main(int argc,char **argv)
{
 unsigned n=argc>1?(unsigned)atoi(argv[1]):0;
 test_nv_reset();if(n==16)g_relay_nv=1;
 assert(app_init());
 if(n<6)
 {
  comm_port_t port=(comm_port_t)(n%3);unsigned i;
  const uint16_t expected[]={0,1310,(uint16_t)-41,100,4200,10000,(uint16_t)-55,1430,208};
  if(n>=3)change(1,100);
  publish_values();
  local_request(port,3,35,9,1100);only_source(port);
  assert(g_tx_len[port]==23&&p_tx[port][2]==18);
  for(i=0;i<9;i++)assert(read_word(p_tx[port]+3+2*i)==expected[i]);
  assert(!g_param_nv_writes&&!g_relay_nv_writes);
  return 0;
 }
 switch(n)
 {
 case 6:
 {
  unsigned mask,requests,i;comm_params_t params=settings();
  for(mask=0;mask<4;mask++)for(requests=0;requests<4;requests++)
  {
   params.relay_contact_mask=(uint8_t)mask;assert(svc_params_set(&params));
   assert(svc_relay_set(0,requests&1)&&svc_relay_set(1,(requests>>1)&1));
   for(i=0;i<2;i++)
   {
    assert(relay_state().output[i]==((requests>>i)&1));
    assert(relay_state().gpio[i]==(((requests^mask)>>i)&1));
    assert(g_relay_gpio[i]==relay_state().gpio[i]);
   }
  }
  assert(!g_param_nv_writes&&!g_relay_nv_writes);break;
 }
 case 7:
  local_request(COMM_PORT_4G,6,35,3,100);
  assert(settings().relay_contact_mask==3&&g_relay_gpio[0]&&g_relay_gpio[1]);
  assert(!relay_state().requested[0]&&!relay_state().requested[1]&&!relay_state().dirty);
  finish_local(COMM_PORT_4G,200);
  local_request(COMM_PORT_4G,3,17,4,300);
  assert(read_word(p_tx[0]+3)==0&&read_word(p_tx[0]+5)==0);
  assert(read_word(p_tx[0]+7)==1&&read_word(p_tx[0]+9)==1);
  finish_local(COMM_PORT_4G,400);
  local_request(COMM_PORT_4G,6,35,4,500);
  assert(p_tx[0][1]==0x86&&p_tx[0][2]==3&&settings().relay_contact_mask==3);
  assert(!g_param_nv_writes&&!g_relay_nv_writes);break;
 case 8:
 case 9:
 {
  unsigned i;if(n==9)change(1,100);
  for(i=0;i<3;i++)
  {
   local_request((comm_port_t)i,6,35,(uint16_t)(i+1),1100+i*200);
   only_source((comm_port_t)i);
   assert(g_relay_gpio[0]==((i+1)&1)&&g_relay_gpio[1]==(((i+1)>>1)&1));
   finish_local((comm_port_t)i,1200+i*200);
  }
  assert(!g_param_nv_writes&&!g_relay_nv_writes);break;
 }
 case 10:
 {
  uint32_t t;comm_params_t p=settings();
  p.relay_contact_mask=3;p.uv_enabled=1;assert(svc_params_set(&p));
  assert(svc_relay_set(0,1)&&svc_relay_set(1,1));
  assert(!g_relay_gpio[0]&&!g_relay_gpio[1]);
  g_test_sampling.valid=1;g_test_sampling.input_mv=9000;
  for(t=10;t<=3010;t+=100)tick(t);
  assert(relay_state().uv_active&&!relay_state().output[0]&&!relay_state().output[1]);
  assert(g_relay_gpio[0]&&g_relay_gpio[1]);
  p.relay_contact_mask=1;assert(svc_params_set(&p));tick(3020);
  assert(g_relay_gpio[0]&&!g_relay_gpio[1]);
  change(1,3100);change(0,4100);
  assert(relay_state().uv_active&&relay_state().protection_entries==1);
  assert(settings().relay_contact_mask==1);
  g_test_sampling.input_mv=13000;
  for(t=5100;t<=8100;t+=100)tick(t);
  assert(!relay_state().uv_active&&relay_state().output[0]&&relay_state().output[1]);
  assert(!g_relay_gpio[0]&&g_relay_gpio[1]);break;
 }
 case 11:
 {
  comm_params_t p=settings();p.relay_contact_mask=2;assert(svc_params_set(&p));
  assert(svc_params_save());
  local_request(COMM_PORT_4G,5,17,0xFF00,100);finish_local(COMM_PORT_4G,200);
  assert(g_relay_nv==1&&g_relay_nv_writes==1);
  svc_params_init();g_relay_gpio[0]=0;g_relay_gpio[1]=0;assert(svc_relay_init());
  assert(settings().relay_contact_mask==2&&relay_state().requested[0]&&!relay_state().requested[1]);
  assert(g_relay_gpio[0]&&g_relay_gpio[1]);break;
 }
 case 12:
  record(1);svc_params_init();assert(settings().app_mode==1&&!settings().relay_contact_mask);
  assert(settings().rs485_1_baud==19200&&settings().uv_protect_mv==10500);
  assert(!g_param_nv_writes);break;
 case 13:
 {
  unsigned mask;
  for(mask=0;mask<4;mask++)
  {
   record(1);g_nv_data[1]|=(uint64_t)mask<<48;record_checksum();svc_params_init();
   assert(settings().relay_contact_mask==mask&&settings().rs485_1_baud==19200);
  }
  record(1);g_nv_data[1]|=(uint64_t)4<<48;record_checksum();svc_params_init();
  assert(!settings().relay_contact_mask&&settings().rs485_1_baud==9600);
  record(1);g_nv_data[1]|=(uint64_t)1<<56;record_checksum();svc_params_init();
  assert(!settings().relay_contact_mask&&settings().rs485_1_baud==9600);break;
 }
 case 14:
  local_request(COMM_PORT_4G,6,35,3,100);finish_local(COMM_PORT_4G,200);
  assert(!g_param_nv_writes&&!g_relay_nv_writes&&g_relay_gpio[0]&&g_relay_gpio[1]);
  svc_params_init();tick(300);
  assert(!settings().relay_contact_mask&&!g_relay_gpio[0]&&!g_relay_gpio[1]);break;
 case 15:
  local_request(COMM_PORT_RS485_2,6,35,3,100);finish_local(COMM_PORT_RS485_2,200);
  local_request(COMM_PORT_RS485_2,6,33,56781,300);
  assert(!g_param_nv_writes&&!g_reset_count);
  finish_local(COMM_PORT_RS485_2,400);
  assert(g_param_nv_writes==1&&g_reset_count==1&&!g_relay_nv_writes);
  assert(((g_nv_data[1]>>48)&255)==3);
  svc_params_init();assert(settings().relay_contact_mask==3);break;
 case 16:
 {
  comm_params_t p=settings();p.relay_contact_mask=3;assert(svc_params_set(&p));
  local_request(COMM_PORT_RS485_1,6,33,52445,100);finish_local(COMM_PORT_RS485_1,200);
  assert(!settings().relay_contact_mask&&!settings().app_mode&&g_reset_count==1);
  assert(g_relay_nv==1&&!g_relay_nv_writes);break;
 }
 case 17:
 {
  uint8_t sn[16]={0};svc_identity_status_t id;sn[15]='X';assert(svc_identity_set_sn(sn,16));
  publish_values();local_request(COMM_PORT_4G,6,35,1,100);finish_local(COMM_PORT_4G,200);
  assert(snapshot().battery_valid&&snapshot().solar_valid);
  change(1,500);assert(settings().relay_contact_mask==1);
  assert(svc_identity_get_status(&id)&&id.sn[15]=='X'&&id.writes==1);break;
 }
 case 18:
 {
  unsigned port,reg;uint32_t t=100;publish_values();
  for(port=0;port<3;port++)for(reg=36;reg<=43;reg++)
  {
   local_request((comm_port_t)port,6,(uint16_t)reg,0,t);
   assert(p_tx[port][1]==0x86&&p_tx[port][2]==2);finish_local((comm_port_t)port,t+100);t+=200;
  }
  for(reg=2;reg<=5;reg++)
  {local_request(COMM_PORT_4G,6,34,(uint16_t)reg,t);assert(p_tx[0][1]==0x86&&p_tx[0][2]==3);finish_local(COMM_PORT_4G,t+100);t+=200;}
  assert(snapshot().battery.voltage_10mv==1310&&!settings().relay_contact_mask);
  assert(!g_param_nv_writes&&!g_relay_nv_writes&&!g_mode.switches);break;
 }
 case 19:
 {
  svc_telemetry_snapshot_t s;
  publish_values();s=snapshot();tick(1000);assert(svc_telemetry_publish_solar(&s.solar));
  local_request(COMM_PORT_4G,3,36,8,HIK_CACHE_STALE_MS+50);
  {unsigned i;for(i=0;i<6;i++)assert(!read_word(p_tx[0]+3+2*i));}
  assert(read_word(p_tx[0]+15)==1430&&read_word(p_tx[0]+17)==208);
  finish_local(COMM_PORT_4G,HIK_CACHE_STALE_MS+150);
  assert(svc_telemetry_publish_battery(&s.battery));
  local_request(COMM_PORT_RS485_1,3,36,8,HIK_CACHE_STALE_MS+300);
  assert(read_word(p_tx[1]+3)==1310&&read_word(p_tx[1]+5)==(uint16_t)-41);break;
 }
 case 20:
 {
  proto_local_snapshot_t s={0};proto_local_command_t c;uint8_t p[]={0x14,3,0,41,0,1,0,0};uint8_t out[7];uint16_t len;
  s.params=settings();s.battery_valid=1;crc(p,6);
  assert(PROTO_LOCAL_REPLY==proto_local_process(p,8,&s,&c,out,7,&len));assert(!read_word(out+3));
  s.battery.ntc_count=1;s.battery.temperature_deci_kelvin[0]=2676;
  assert(PROTO_LOCAL_REPLY==proto_local_process(p,8,&s,&c,out,7,&len));assert(read_word(out+3)==(uint16_t)-55);
  s.battery.temperature_deci_kelvin[0]=65535;
  assert(PROTO_LOCAL_REPLY==proto_local_process(p,8,&s,&c,out,7,&len));assert(!read_word(out+3));
  p[3]=37;crc(p,6);s.battery.current_10ma=INT16_MIN;
  assert(PROTO_LOCAL_REPLY==proto_local_process(p,8,&s,&c,out,7,&len));assert(read_word(out+3)==0x8000);
  s.battery.current_10ma=INT16_MAX;
  assert(PROTO_LOCAL_REPLY==proto_local_process(p,8,&s,&c,out,7,&len));assert(read_word(out+3)==0x7FFF);break;
 }
 case 21:
 {
  proto_local_snapshot_t s={0};proto_local_command_t c;uint8_t p[]={0x14,3,0,17,0,27,0,0};uint8_t out[60];uint16_t len;
  s.params=settings();crc(p,6);memset(out,0xA5,60);
  assert(PROTO_LOCAL_DROP==proto_local_process(p,8,&s,&c,out,58,&len)&&!len);
  assert(out[58]==0xA5&&out[59]==0xA5);
  assert(PROTO_LOCAL_REPLY==proto_local_process(p,8,&s,&c,out,59,&len));assert(len==59&&out[2]==54&&out[59]==0xA5);
  p[5]=28;crc(p,6);assert(PROTO_LOCAL_REPLY==proto_local_process(p,8,&s,&c,out,59,&len));assert(out[1]==0x83&&out[2]==2);
  p[3]=43;p[5]=1;crc(p,6);assert(PROTO_LOCAL_REPLY==proto_local_process(p,8,&s,&c,out,59,&len));assert(len==7);
  p[3]=44;crc(p,6);assert(PROTO_LOCAL_REPLY==proto_local_process(p,8,&s,&c,out,59,&len));assert(out[1]==0x83&&out[2]==2);break;
 }
 case 22:
 {
  uint32_t t;comm_params_t p=settings();p.uv_enabled=1;assert(svc_params_set(&p));
  g_test_sampling.valid=1;g_test_sampling.input_mv=9000;
  for(t=10;t<=2010;t+=100)tick(t);
  local_request(COMM_PORT_4G,6,35,2,2110);finish_local(COMM_PORT_4G,2210);
  for(t=2310;t<=3010;t+=100)tick(t);
  assert(relay_state().uv_active&&relay_state().protection_entries==1);
  assert(!g_relay_gpio[0]&&g_relay_gpio[1]);break;
 }
 case 23:
  local_request(COMM_PORT_4G,6,35,3,100);finish_local(COMM_PORT_4G,200);
  local_request(COMM_PORT_4G,5,17,0xFF00,300);finish_local(COMM_PORT_4G,400);
  assert(g_relay_nv==1&&g_relay_nv_writes==1&&!g_param_nv_writes);
  assert(relay_state().requested[0]&&relay_state().output[0]&&!g_relay_gpio[0]&&g_relay_gpio[1]);break;
 default:abort();
 }
 return 0;
}
'''


class ContactTelemetryTests(unittest.TestCase):
    def test_contact_telemetry(self):
        compiler=shutil.which('gcc')
        if not compiler:
            raise RuntimeError('Native GCC required')
        names=[f'35..43 signed telemetry read, {mode}, UART{port}'
               for mode in ['DTO','HIK'] for port in [1,2,3]]
        names+=['all four contact masks and all logical request combinations',
                'live polarity, logical readback and invalid mask rejection',
                'contact write on all DTO ports','contact write on all HIK ports',
                'UV entry/recovery and mode transitions use same polarity mapping',
                'saved wiring and logical relay memory restored together',
                'old records retain parameters and default NC wiring',
                'stored contact masks and reserved byte validation',
                'unsaved wiring resets without erasing relay memory',
                'explicit save only after UART3 ACK persists wiring',
                'defaults clear wiring but keep independent relay page',
                'wiring write preserves telemetry and SN survives mode switch',
                'telemetry writes and future mode numbers rejected on all ports',
                'independent expiry zeros measurements then recovers',
                'temperature/NTC and signed current boundaries',
                '59-byte complete map and address/capacity boundaries',
                'contact change does not reset UV dwell',
                'automatic relay memory stores logical request, not GPIO']
        with tempfile.TemporaryDirectory(prefix='comm-convert-contact-') as tmp:
            directory=pathlib.Path(tmp)
            (directory/'usart.h').write_text(HAL_HEADER,encoding='utf-8')
            source=HARNESS.replace('int main(int argc, char **argv)',
                                    'int serial_base_main(int argc, char **argv)')
            source=source.replace('static uint32_t g_reset_count;',
                                  'static uint32_t g_reset_count;\nstatic unsigned g_param_nv_writes,g_mode_flash_fail;')
            source=source.replace('memcpy(g_nv_data, p_words, 24U);',
                                  'g_param_nv_writes++;if(g_mode_flash_fail)return 0U;\nmemcpy(g_nv_data, p_words, 24U);')
            for harness,name,signature in [(DTO_HARNESS,'dto','int main(int argc, char **argv)'),
                                            (POLL_HARNESS,'poll','int main(int argc,char **argv)'),
                                            (MODE_HARNESS,'mode','int main(int argc,char **argv)'),
                                            (UNIFIED_HARNESS,'unified','int main(int argc,char **argv)')]:
                source+=harness.replace(signature,f'int {name}_base_main(int argc,char **argv)')
            harness_file=directory/'harness.c'
            harness_file.write_text(source+EXTENDED_HARNESS,encoding='utf-8')
            executable=directory/'contact.exe'
            command=[compiler,'-std=c99','-Wall','-Wextra','-Werror']
            for include in (directory,*(ROOT/p for p in ['Config','Bsp','Services','App','Protocols'])):
                command+=['-I',str(include)]
            paths=['Bsp/bsp_uart.c','Services/svc_serial.c','Protocols/proto_gm800.c',
                   'Services/svc_gm800.c','Services/svc_params.c','Services/svc_boot.c','Config/comm_params.c',
                   'App/app_dto.c','App/app_main.c','App/app_local.c','App/app_mode.c',
                   'Protocols/proto_local.c','Services/svc_identity.c','Services/svc_relay.c',
                   'App/app_hik.c','Protocols/proto_hik.c','Services/svc_telemetry.c',
                   'Services/svc_poll.c','Protocols/proto_controller.c']
            command += [str(ROOT/p) for p in paths]+[str(harness_file),'-o',str(executable)]
            subprocess.run(command,check=True,timeout=60)
            for index,name in enumerate(names):
                with self.subTest(scenario=name):
                    subprocess.run([str(executable),str(index)],check=True,timeout=10)
                    print('PASS:',name)


if __name__=='__main__':
    unittest.main(verbosity=2)
