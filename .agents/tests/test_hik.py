"""HIK host integration and byte-for-byte comparison to approved packet fixtures."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

from test_serial import HAL_HEADER, HARNESS, ROOT
from test_dto import DTO_HARNESS
from test_local import LOCAL_HARNESS

REFERENCE_HEADER=r'''
#include <stdint.h>
struct bat_info {
 uint16_t voltage;float current;uint16_t ream_cap,normal_cap,cycle_count,data_manu;
 uint16_t equil_state_low,equil_state_high,protect_state;
 uint8_t soft_version,bat_rsoc,fet_state,cell_num,ntc_num;float ntc_temp[5];
};
struct sol_info {uint8_t num_sol;uint16_t chg_voltage;float chg_current;};
extern struct bat_info xbattery_info;
extern struct sol_info xsol_info;
uint8_t golden_read_response(uint8_t *p,uint8_t *out,uint16_t *len);
'''

HIK_HARNESS=r'''
#include "app_hik.h"
#include "proto_hik.h"
#include "svc_telemetry.h"
#include "svc_relay.h"
#include "svc_gm800.h"
#include "svc_identity.h"
#include "hik_config.h"
#include "golden_reference.h"
static app_hik_status_t hik_status(void)
{app_hik_status_t s;assert(app_hik_get_status(&s));return s;}
static void request(uint8_t command,uint8_t *out)
{
 out[0]=0xDDU;out[1]=0xA5U;out[2]=command;out[3]=0U;
 out[4]=0xFFU;out[5]=(uint8_t)(0U-command);out[6]=0x77U;
}
static void input(comm_port_t port,const uint8_t *data,uint16_t len,uint32_t time)
{
 unsigned i;for(i=0;i<len;i++)inject(port,data[i],time+i);
 g_now=time+len-1U+(port==COMM_PORT_4G?15U:50U);
 dto_step();dto_step();dto_step();
}
static void host(uint8_t command,uint32_t time)
{uint8_t p[7];request(command,p);input(COMM_PORT_RS485_1,p,7U,time);}
static void done(void)
{assert(p_tx[1U]);complete(COMM_PORT_RS485_1);dto_step();}
static uint16_t sn_frame(uint8_t *out,const uint8_t *sn,uint8_t length)
{
 uint16_t sum;
 out[0]=0xDD;out[1]=0x5A;out[2]=0xA3;out[3]=length;
 if(length)memcpy(out+4U,sn,length);
 sum=proto_hik_checksum(out+2U,length+2U);
 out[length+4U]=(uint8_t)(sum>>8U);out[length+5U]=(uint8_t)sum;
 out[length+6U]=0x77;
 return length+7U;
}
static svc_identity_status_t sn_status(void)
{svc_identity_status_t data;assert(svc_identity_get_status(&data));return data;}
static void sn_write(const uint8_t *sn,uint8_t length,uint32_t time)
{uint8_t frame[24U];uint16_t size=sn_frame(frame,sn,length);input(COMM_PORT_RS485_1,frame,size,time);}
static void publish(void)
{
 comm_battery_data_t b={0};comm_solar_data_t s={0};
 b.voltage_10mv=2600;b.current_10ma=-100;b.remaining_10mah=5000;
 b.nominal_10mah=10000;b.software_version=0x10;b.soc_percent=67;
 b.fet_state=3;b.ntc_count=2;b.temperature_deci_kelvin[0]=2981;
 b.temperature_deci_kelvin[1]=2681;
 s.number=1;s.voltage_10mv=1800;s.current_10ma=100;
 assert(svc_telemetry_publish_battery(&b)&&svc_telemetry_publish_solar(&s));
 memset(&xbattery_info,0,sizeof(xbattery_info));memset(&xsol_info,0,sizeof(xsol_info));
 xbattery_info.voltage=2600;xbattery_info.current=-1.0f;xbattery_info.ream_cap=5000;
 xbattery_info.normal_cap=100;xbattery_info.bat_rsoc=67;xbattery_info.ntc_num=2;
 xbattery_info.ntc_temp[0]=25;xbattery_info.ntc_temp[1]=-5;
 xsol_info.chg_voltage=1800;xsol_info.chg_current=1.0f;
}
static void assert_reference(uint8_t command)
{
 uint8_t in[7],old[128]={0};uint16_t length=0;
 request(command,in);assert(golden_read_response(in,old,&length));
 assert(g_tx_len[1U]==length&&!memcmp(old,p_tx[1U],length));
}
int main(int argc,char **argv)
{
 int n=argc>1?atoi(argv[1]):0;
 test_nv_reset();assert(app_init());
 switch(n)
 {
 case 0:
  {const uint8_t golden[]={0xDD,0x0C,0,5,4,1,1,0,0,0xFF,0xF5,0x77};
   host(0x0CU,100U);assert(g_tx_len[1U]==sizeof(golden));
   assert(!memcmp(golden,p_tx[1U],sizeof(golden)));assert_reference(0x0CU);
   assert(!p_tx[0U]&&!p_tx[2U]&&hik_status().replies_queued==1U);done();}
  break;
 case 1:
  publish();host(3U,100U);assert_reference(3U);assert(g_tx_len[1U]==34U);done();
  host(15U,300U);assert_reference(15U);done();break;
 case 2:
  {svc_telemetry_snapshot_t data;
   host(3U,100U);assert_reference(3U);assert(g_tx_len[1U]==30U);done();
   host(15U,300U);assert_reference(15U);done();
   assert(hik_status().no_data==2U&&hik_status().replies_queued==2U);
   assert(svc_telemetry_get(&data)&&!data.battery_valid&&!data.solar_valid);}
  break;
 case 3:
  {svc_telemetry_snapshot_t data;publish();g_now=HIK_CACHE_STALE_MS-1U;
   assert(svc_telemetry_get(&data)&&data.battery_valid&&data.solar_valid);
   g_now=HIK_CACHE_STALE_MS;assert(svc_telemetry_get(&data)&&!data.battery_valid&&!data.solar_valid);
   assert(svc_telemetry_publish_solar(&data.solar));
   assert(svc_telemetry_get(&data)&&!data.battery_valid&&data.solar_valid);
   host(3U,HIK_CACHE_STALE_MS+100U);assert(p_tx[1U]&&g_tx_len[1U]==30U);
   assert(p_tx[1U][4U]==0U&&p_tx[1U][5U]==0U);done();
   host(15U,HIK_CACHE_STALE_MS+300U);assert_reference(15U);done();}
  break;
 case 4:
  {uint8_t p[7];request(12,p);p[5]^=1U;input(COMM_PORT_RS485_1,p,7,100);
   assert(!p_tx[1U]&&hik_status().invalid_frames==1U);
   request(12,p);p[6]=0;input(COMM_PORT_RS485_1,p,7,300);
   assert(hik_status().invalid_frames==2U);host(12,500);assert(p_tx[1U]);done();}
  break;
 case 5:
  {uint8_t p[14];request(12,p);memcpy(p+7,p,7);
   input(COMM_PORT_RS485_1,p,14,100);assert(!p_tx[1U]);
   input(COMM_PORT_RS485_1,p,3,300);assert(!p_tx[1U]);
   assert(hik_status().invalid_frames==2U);host(12,500);done();}
  break;
 case 6:
  {uint8_t p[7];request(0x99,p);input(COMM_PORT_RS485_1,p,7,100);
   request(0xA4,p);p[1]=0x5A;input(COMM_PORT_RS485_1,p,7,300);
   assert(hik_status().unsupported_frames==2U&&!p_tx[1U]&&!g_relay_nv_writes);}
  break;
 case 7:
  {uint8_t p[7];request(12,p);input(COMM_PORT_RS485_2,p,7,100);
   assert(!p_tx[0U]&&!p_tx[1U]&&!p_tx[2U]);
   input(COMM_PORT_4G,p,7,300);assert(!p_tx[0U]&&!p_tx[1U]&&!p_tx[2U]);}
  break;
 case 8:
  {uint8_t p[]={0x14,5,0,17,0xFF,0,0,0};send_request(p,8,100);
   assert(p_tx[0U]&&g_relay_gpio[0U]&&!g_relay_nv_writes);finish_reply();
   assert(g_relay_nv_writes==1U&&g_relay_nv==1U&&!g_reset_count);
   assert(APP_LOCAL_DONE==local_status().state);
   host(12,400);assert(p_tx[1U]);done();}
  break;
 case 9:
  {uint8_t read[]={0x14,3,0,26,0,8,0,0};send_request(read,8,100);
   assert(p_tx[0U]&&g_tx_len[0U]==21U);
   assert(reg16(p_tx[0U]+3U)==params().uv_protect_mv);finish_reply();
   host(12,300);assert(p_tx[1U]);done();}
  break;
 case 10:
  {const uint8_t p[]="AT+CLOUD=01234567890123456789";
   input(COMM_PORT_RS485_1,p,sizeof(p)-1U,100);
   assert(svc_gm800_busy()&&hik_status().configuration_requests==1U);
   assert(!p_tx[1U]&&!p_tx[2U]);}
  break;
 case 11:
  {const uint8_t p[]="AT+CLOUD=01234567890123456789";
   input(COMM_PORT_RS485_2,p,sizeof(p)-1U,100);
   assert(svc_gm800_busy()&&hik_status().configuration_requests==1U);
   assert(!p_tx[1U]&&!p_tx[2U]);}
  break;
 case 12:
  {uint8_t p[7],old[200]={0xAA};unsigned i;request(12,p);
   for(i=0;i<4;i++)assert(SVC_SERIAL_OK==svc_serial_send(COMM_PORT_RS485_1,old,200));
   input(COMM_PORT_RS485_1,p,7,100);assert(hik_status().blocked_attempts);
   assert(hik_status().replies_queued==0U);complete(COMM_PORT_RS485_1);dto_step();
   assert(hik_status().replies_queued==1U);}
  break;
 case 13:
  {uint8_t p[7],old=0;unsigned i;request(12,p);
   for(i=0;i<4;i++)assert(SVC_SERIAL_OK==svc_serial_send(COMM_PORT_RS485_1,&old,1));
   for(i=0;i<7;i++)inject(COMM_PORT_RS485_1,p[i],100+i);
   g_now=156;svc_serial_task();assert(app_hik_task());assert(hik_status().blocked_attempts);
   g_now+=1000;assert(app_hik_task());assert(hik_status().queue_timeouts==1U);}
  break;
 case 14:
  {uint8_t p[7],old=0;unsigned i;uint8_t control[]={0x14,5,0,17,0xFF,0,0,0};
   request(12,p);for(i=0;i<4;i++)assert(SVC_SERIAL_OK==svc_serial_send(COMM_PORT_RS485_1,&old,1));
   for(i=0;i<7;i++)inject(COMM_PORT_RS485_1,p[i],100+i);
   g_now=156;svc_serial_task();assert(app_hik_task());crc_frame(control,6);
   for(i=0;i<8;i++)inject(COMM_PORT_4G,control[i],200+i);
   g_now=222;svc_serial_task();assert(app_hik_task());assert(hik_status().maintenance_dropped==1U);}
  break;
 case 15:
  {svc_telemetry_snapshot_t data;g_now=0xFFFFFF00U;publish();g_now+=500U;
   assert(svc_telemetry_get(&data)&&data.battery_valid);svc_telemetry_invalidate();
   assert(svc_telemetry_get(&data)&&!data.battery_valid&&!data.solar_valid);}
  break;
 case 16:
  {comm_battery_data_t b={0};uint8_t out[40];uint8_t c;uint8_t p[7];
   b.ntc_count=5;assert(proto_hik_build_battery(&b,out,40)==40U);
   assert(!proto_hik_build_battery(&b,out,39));b.ntc_count=6;
   assert(!svc_telemetry_publish_battery(&b)&&!proto_hik_build_battery(&b,out,40));
   b.ntc_count=0;b.soc_percent=101;assert(!svc_telemetry_publish_battery(&b));
   assert(!svc_telemetry_get(NULL)&&!app_hik_get_status(NULL));
   assert(!proto_hik_build_info(out,11)&&!proto_hik_build_info(NULL,12));
   request(12,p);assert(PROTO_HIK_INVALID==proto_hik_parse(p,7,NULL));
   assert(PROTO_HIK_NOT_HIK==proto_hik_parse(NULL,0,&c));}
  break;
 case 17:
  {comm_battery_data_t b={0};uint8_t out[40];b.current_10ma=INT16_MIN;
   assert(proto_hik_build_battery(&b,out,40)==30U);
   assert(out[6]==0x80U&&out[7]==0U);
   b.current_10ma=INT16_MAX;assert(proto_hik_build_battery(&b,out,40)==30U);
   assert(out[6]==0x7FU&&out[7]==0xFFU);}
  break;
 case 18:
  {uint8_t p[]={0xDD,0xA5,0x0F,1,1,0xFF,0xEF,0x77};
   uint8_t old[128]={0};uint16_t length=0;uint8_t command=0;
   assert(PROTO_HIK_READ==proto_hik_parse(p,sizeof(p),&command)&&command==0x0F);
   publish();input(COMM_PORT_RS485_1,p,sizeof(p),100);
   assert(golden_read_response(p,old,&length));
   assert(length==g_tx_len[1U]&&!memcmp(old,p_tx[1U],length));done();}
  break;
 case 19:
  {uint8_t p[]={0xDD,0xA5,0x0C,1,1,0xFF,0xF2,0x77};
   input(COMM_PORT_RS485_1,p,sizeof(p),100);
   assert_reference(12U);assert(hik_status().invalid_frames==0U);done();}
  break;
 case 20:
  {uint8_t p[256]={0xDD,0xA5,0x0C,249U};uint8_t command=0;
   uint16_t sum=proto_hik_checksum(p+2U,251U);
   p[253]=(uint8_t)(sum>>8U);p[254]=(uint8_t)sum;p[255]=0x77;
   assert(PROTO_HIK_READ==proto_hik_parse(p,sizeof(p),&command));
   assert(PROTO_HIK_INVALID==proto_hik_parse(p,257U,&command));
   input(COMM_PORT_RS485_1,p,sizeof(p),100);assert_reference(12U);done();}
  break;
 case 21:
  {uint8_t p[]={0xDD,0xA5,0x0F,1,1,0xFF,0xEF,0x77};
   input(COMM_PORT_RS485_2,p,sizeof(p),100);
   assert(!p_tx[1U]&&!p_tx[2U]&&hik_status().requests==0U);
   input(COMM_PORT_RS485_1,p,sizeof(p),300);
   assert(hik_status().requests==1U&&hik_status().no_data==1U);
   assert(hik_status().invalid_frames==0U&&p_tx[1U]);assert_reference(15U);done();}
  break;
 case 22:
  {svc_telemetry_snapshot_t data;unsigned i;
   publish();g_now=HIK_CACHE_STALE_MS+1000U;host(3U,HIK_CACHE_STALE_MS+1100U);
   assert(p_tx[1U]&&g_tx_len[1U]==30U);
   for(i=4U;i<27U;i++)assert(p_tx[1U][i]==(i==22U?0x10U:0U));
   assert(svc_telemetry_get(&data)&&!data.battery_valid&&data.battery.voltage_10mv==2600U);
   done();host(15U,HIK_CACHE_STALE_MS+1300U);assert(p_tx[1U][4U]==1U);
   for(i=5U;i<9U;i++)assert(p_tx[1U][i]==0U);
   done();publish();host(3U,HIK_CACHE_STALE_MS+1500U);assert_reference(3U);done();}
  break;
 case 23:
  {const uint8_t value[]="1234567890123456";
   const uint8_t ack[]={0xDD,0xA3,0,0,0,0,0x77};
   uint8_t frame[24];
   uint16_t size=sn_frame(frame,value,16);svc_identity_status_t data;
   input(COMM_PORT_RS485_1,frame,size,100);data=sn_status();
   assert(data.length==16&&data.writes==1&&!memcmp(data.sn,value,16)&&data.sn[16]==0);
   assert(g_tx_len[1U]==7&&!memcmp(p_tx[1U],ack,7));
   assert(!g_sn_param_flash_calls&&!g_relay_nv_writes&&!g_reset_count);done();}
  break;
 case 24:
  {const uint8_t value[]="1234567890123456";
   uint8_t expected[23]={0xDD,0x0D,0,16};uint16_t sum;
   sn_write(value,16,100);done();host(13,300);
   memcpy(expected+4U,value,16);sum=proto_hik_checksum(expected+2U,18);
   expected[20]=(uint8_t)(sum>>8U);expected[21]=(uint8_t)sum;expected[22]=0x77;
   assert(g_tx_len[1U]==23&&!memcmp(p_tx[1U],expected,23));done();}
  break;
 case 25:
  {const uint8_t full[]="1234567890123456";
   const uint8_t shorter[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,'A','B','C'};
   unsigned i;svc_identity_status_t data;
   sn_write(full,16,100);done();sn_write(shorter,16,300);done();data=sn_status();
   assert(data.length==16&&data.writes==2&&!memcmp(data.sn,shorter,16));
   for(i=0;i<13;i++)assert(data.sn[i]==0);
   assert(data.sn[16]==0);
   assert(!g_sn_param_flash_calls&&!g_relay_nv_writes&&!g_reset_count);}
  break;
 case 26:
  {const uint8_t value[]="1234567890123456";uint8_t frame[24];uint16_t size;
   sn_write(value,16,100);done();size=sn_frame(frame,value,16);
   frame[20]^=1U;input(COMM_PORT_RS485_1,frame,size,300);
   assert(sn_status().writes==1&&!memcmp(sn_status().sn,value,16)&&!p_tx[1U]);
   size=sn_frame(frame,value,16);input(COMM_PORT_RS485_1,frame,size-1U,500);
   assert(sn_status().writes==1&&!p_tx[1U]);}
  break;
 case 27:
  {uint8_t frame[24],value[17]={0};uint16_t size;
   size=sn_frame(frame,value,0);input(COMM_PORT_RS485_1,frame,size,100);
   size=sn_frame(frame,value,17);input(COMM_PORT_RS485_1,frame,size,300);
   size=sn_frame(frame,value,15);input(COMM_PORT_RS485_1,frame,size,500);
   assert(sn_status().length==0&&sn_status().writes==0&&!p_tx[1U]);
   assert(hik_status().invalid_frames==3U);}
  break;
 case 28:
  {const uint8_t empty[]={0xDD,0x0D,0,0,0,0,0x77};
   host(13,100);assert(g_tx_len[1U]==7&&!memcmp(p_tx[1U],empty,7));done();
   sn_write((const uint8_t *)"1234567890123456",16,300);done();svc_identity_init();
   assert(sn_status().length==0&&sn_status().writes==0);
   host(13,500);assert(g_tx_len[1U]==7&&!memcmp(p_tx[1U],empty,7));done();}
  break;
 case 29:
  {uint8_t frame[24];uint16_t size=sn_frame(frame,(const uint8_t *)"1234567890123456",16);
   input(COMM_PORT_RS485_2,frame,size,100);input(COMM_PORT_4G,frame,size,300);
   assert(sn_status().writes==0&&!p_tx[0U]&&!p_tx[1U]&&!p_tx[2U]);}
  break;
 case 30:
  {uint8_t out[24],value[17]={0};
   assert(!svc_identity_get_status(NULL)&&!svc_identity_set_sn(NULL,3));
   assert(!svc_identity_set_sn(value,0)&&!svc_identity_set_sn(value,17));
   assert(!svc_identity_set_sn(value,15));
   assert(!proto_hik_build_sn(value,17,out,sizeof(out)));
   assert(!proto_hik_build_sn(NULL,1,out,sizeof(out)));
   assert(!proto_hik_build_sn(value,16,out,22));
   assert(!proto_hik_build_sn_ack(out,6)&&!proto_hik_build_sn_ack(NULL,7));
   assert(proto_hik_build_sn_ack(out,7)==7U);
   assert(sn_status().writes==0);}
  break;
 case 31:
  {uint8_t old[200]={0};unsigned i;
   for(i=0;i<4;i++)assert(SVC_SERIAL_OK==svc_serial_send(COMM_PORT_RS485_1,old,200));
   sn_write((const uint8_t *)"1234567890123456",16,100);
   assert(sn_status().writes==1&&hik_status().blocked_attempts);
   complete(COMM_PORT_RS485_1);dto_step();assert(sn_status().writes==1);
   assert(hik_status().replies_queued==1&&!g_sn_param_flash_calls&&!g_relay_nv_writes);}
  break;
 case 32:
  {const uint8_t frame[]={0xDD,0x5A,0xA3,0x10,0,0,0,0,0,0,0,
                         0x41,0x42,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0xFD,0x5E,0x77};
   const uint8_t ack[]={0xDD,0xA3,0,0,0,0,0x77};svc_identity_status_t data;
   input(COMM_PORT_RS485_1,frame,sizeof(frame),100);data=sn_status();
   assert(data.length==16&&!memcmp(data.sn,frame+4U,16));
   assert(!memcmp(data.sn+7U,"AB1234567",9));
   assert(g_tx_len[1U]==7&&!memcmp(p_tx[1U],ack,7));done();
   host(13,300);assert(g_tx_len[1U]==23&&!memcmp(p_tx[1U]+4U,frame+4U,16));done();
   assert(!g_sn_param_flash_calls&&!g_relay_nv_writes&&!g_reset_count);}
  break;
 default:assert(0);
 }
 return 0;
}
'''


def reference_functions():
    # Frozen approved replies keep migration tests runnable without app-demo/.
    # Do not obtain expected bytes from the production protocol builders.
    return r"""
#include <string.h>
#include "golden_reference.h"
struct bat_info xbattery_info;
struct sol_info xsol_info;
/* Approved fixed packet fixtures, independent of production builders. */
static const uint8_t info[] = {0xDD,0x0C,0x00,0x05,0x04,0x01,0x01,0x00,0x00,0xFF,0xF5,0x77};
static const uint8_t empty_battery[] = {0xDD,0x03,0x00,0x17,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x00,0xFF,0xD9,0x77};
static const uint8_t battery[] = {0xDD,0x03,0x00,0x1B,0x0A,0x28,0xFF,0x9C,0x13,0x88,0x27,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x43,0x03,0x00,0x02,0x0B,0xA5,0x0A,0x79,0xFB,0xBB,0x77};
static const uint8_t empty_solar[] = {0xDD,0x0F,0x00,0x05,0x01,0x00,0x00,0x00,0x00,0xFF,0xFA,0x77};
static const uint8_t solar[] = {0xDD,0x0F,0x00,0x05,0x01,0x07,0x08,0x00,0x64,0xFF,0x87,0x77};
uint8_t golden_read_response(uint8_t *p,uint8_t *out,uint16_t *len)
{
 const uint8_t *data;uint16_t size;
 if(p[2]==12){data=info;size=sizeof(info);}
 else if(p[2]==3){data=xbattery_info.voltage?battery:empty_battery;
                  size=xbattery_info.voltage?sizeof(battery):sizeof(empty_battery);}
 else if(p[2]==15){data=xsol_info.chg_voltage?solar:empty_solar;
                   size=xsol_info.chg_voltage?sizeof(solar):sizeof(empty_solar);}
 else return 0;
 memcpy(out,data,size);*len=size;return 1;
}
"""


class HikTests(unittest.TestCase):
    def test_hik(self):
        compiler=shutil.which('gcc')
        if compiler is None:
            raise RuntimeError('Native GCC required')
        names=['0C fixed golden reply',
               '03/0F byte-exact approved golden replies', 'zero-data replies match legacy startup',
               'independent cache expiry', 'bad checksum/end and recovery',
               'combined and short frames rejected', 'unknown commands rejected',
               'port ownership, no DTO broadcast', 'shared relay save and no reset',
               'shared UART1 parameter reads', '485_1 config entry', '485_2 config entry',
               'full queue retains reply', 'queue wait bounded', 'maintenance drops old reply',
               'cache tick wrap and invalidate', 'capacity/NTC/null guards',
               'signed current extremes','observed 0F optional payload matches golden reply',
               '0C optional payload without external cache','maximum optional payload',
               'observed frame port isolation and zero-data reply',
               'expired measurements zeroed then resume real data',
               '16-byte SN in RAM and specified seven-byte ACK',
               'SN readback frame and checksum', 'zero-padded SN replaces old bytes',
               'bad checksum/truncated SN preserves previous value',
               'non-16-byte SN rejected', 'SN empty at startup and after reset',
               'SN write only on host port', 'SN codec and cache guards',
               'SN write once under response queue congestion',
               'user AB1234567 frame, zero padding, ACK and readback']
        with tempfile.TemporaryDirectory(prefix='comm-convert-hik-') as tmp:
            directory=pathlib.Path(tmp)
            (directory/'usart.h').write_text(HAL_HEADER,encoding='utf-8')
            (directory/'golden_reference.h').write_text(REFERENCE_HEADER,encoding='utf-8')
            reference=directory/'reference.c'
            reference.write_text(reference_functions(),encoding='utf-8')
            obj=directory/'reference.o'
            subprocess.run([compiler,'-std=c99','-w','-c',str(reference),'-o',str(obj)],
                           check=True,timeout=60)
            source=HARNESS.replace('int main(int argc, char **argv)',
                                   'int serial_base_main(int argc, char **argv)')
            source=source.replace('static uint32_t g_reset_count;',
                                  'static uint32_t g_reset_count;\nstatic unsigned g_sn_param_flash_calls;')
            source=source.replace('memcpy(g_nv_data, p_words, 24U);',
                                  'g_sn_param_flash_calls++;\n    memcpy(g_nv_data, p_words, 24U);')
            source+=DTO_HARNESS.replace('int main(int argc, char **argv)',
                                        'int dto_base_main(int argc, char **argv)')
            source+=LOCAL_HARNESS.split('static void load_saved(void)')[0]
            harness=directory/'harness.c'
            harness.write_text(source+HIK_HARNESS,encoding='utf-8')
            executable=directory/'hik.exe'
            command=[compiler, '-DCOMM_APP_ENABLE_MODE=0','-std=c99','-Wall','-Wextra','-Werror',
                     '-DCOMM_APP_ENABLE_POLL=0']
            for include in (directory,ROOT/'Config',ROOT/'Bsp',ROOT/'Services',
                            ROOT/'App',ROOT/'Protocols'):
                command+=['-I',str(include)]
            paths=['Bsp/bsp_uart.c','Services/svc_serial.c','Protocols/proto_gm800.c',
                   'Services/svc_gm800.c','Services/svc_params.c','Services/svc_boot.c','Config/comm_params.c',
                   'App/app_dto.c','App/app_main.c','App/app_local.c',
                   'Protocols/proto_local.c','Services/svc_identity.c','Services/svc_relay.c',
                   'App/app_hik.c','Protocols/proto_hik.c','Services/svc_telemetry.c']
            command += [str(ROOT/p) for p in paths]+[str(harness),str(obj),'-o',str(executable)]
            subprocess.run(command,check=True,timeout=60)
            for index,name in enumerate(names):
                with self.subTest(scenario=name):
                    subprocess.run([str(executable),str(index)],check=True,timeout=10)
                    print('PASS:',name)


if __name__=='__main__':
    unittest.main(verbosity=2)
