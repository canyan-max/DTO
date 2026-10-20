"""BootLoad routing, layout rejection and dedicated boot-page hardware boundary."""
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
from test_board_storage import HEADER as FLASH_HEADER

BOOT_HARNESS = r"""
#include "svc_boot.h"
#include "boot_config.h"
static void boot_command(comm_port_t port, uint32_t now)
{const uint8_t command[]="BootLoad";input(port,command,8,now);}
int main(int argc,char **argv)
{
 unsigned n=argc>1?(unsigned)atoi(argv[1]):0;unsigned i;
 svc_boot_status_t bs;app_local_status_t local;
 test_nv_reset();
 if(n>=11&&n<=13)g_test_boot_layout=1;
 if(n==12)g_test_boot_flag=BOOT_FLAG_NORMAL;
 if(n==13)g_test_boot_fail=1;
 assert(app_init());
 if(n<6)
 {
  comm_port_t port=(comm_port_t)(n%3);
  if(n>=3)change(1,100);
  g_test_boot_layout=1;
  boot_command(port,1100);tick(1200);
  assert(g_test_boot_writes==1&&g_test_boot_flag==BOOT_FLAG_REQUEST);
  assert(g_reset_count==1);
  assert(svc_boot_get_status(&bs)&&bs.requests==1&&!bs.failures);
  assert(!g_param_nv_writes&&!g_relay_nv_writes);
  for(i=0;i<3;i++)assert(!p_tx[i]&&!p_rx[i]);
  return 0;
 }
 switch(n)
 {
 case 6:
  for(i=0;i<3;i++)
  {
   boot_command((comm_port_t)i,100+i*200);
   assert(app_local_get_status(&local)&&local.state==APP_LOCAL_FAILED);
  }
  assert(!g_test_boot_writes&&!g_reset_count);
  assert(svc_boot_get_status(&bs)&&bs.layout_rejects==3);break;
 case 7:
 {
  const uint8_t old[]={0xAA,0x55};g_test_boot_layout=1;
  for(i=0;i<3;i++)assert(SVC_SERIAL_OK==svc_serial_send((comm_port_t)i,old,2));
  tick(1);boot_command(COMM_PORT_4G,10);
  assert(!g_test_boot_writes&&app_local_busy());
  complete(COMM_PORT_4G);tick(40);assert(!g_test_boot_writes);
  complete(COMM_PORT_RS485_1);tick(41);assert(!g_test_boot_writes);
  complete(COMM_PORT_RS485_2);tick(42);
  assert(g_test_boot_writes==1&&g_reset_count==1);break;
 }
 case 8:
 {
  const uint8_t old=0xAA;g_test_boot_layout=1;g_force_busy=1;
  assert(SVC_SERIAL_OK==svc_serial_send(COMM_PORT_RS485_2,&old,1));
  tick(1);boot_command(COMM_PORT_4G,10);tick(1002);
  assert(!g_test_boot_writes&&!g_reset_count);
  assert(app_local_get_status(&local)&&local.state==APP_LOCAL_FAILED);break;
 }
 case 9:
  g_test_boot_layout=1;g_test_boot_fail=1;boot_command(COMM_PORT_RS485_1,100);
  assert(g_test_boot_writes==1&&!g_reset_count&&!app_local_busy());
  for(i=0;i<3;i++)assert(p_rx[i]);
  tick(1000);assert(g_test_boot_writes==1);
  g_test_boot_fail=0;boot_command(COMM_PORT_RS485_2,1200);
  assert(g_test_boot_writes==2&&g_reset_count==1);break;
 case 10:
  g_test_boot_layout=1;g_test_boot_read_fail=1;
  boot_command(COMM_PORT_4G,100);
  assert(g_test_boot_writes==1&&!g_reset_count&&!app_local_busy());
  tick(1000);assert(g_test_boot_writes==1);break;
 case 11:
  assert(g_test_boot_writes==1&&g_test_boot_flag==BOOT_FLAG_NORMAL);
  assert(svc_boot_get_status(&bs)&&bs.startup_marks==1&&!bs.failures);break;
 case 12:
  assert(!g_test_boot_writes&&g_test_boot_flag==BOOT_FLAG_NORMAL);
  assert(svc_boot_get_status(&bs)&&!bs.startup_marks&&!bs.failures);break;
 case 13:
  assert(g_test_boot_writes==1&&!g_reset_count);
  assert(svc_boot_get_status(&bs)&&bs.failures==1);break;
 case 14:
  assert(proto_local_is_boot_request((const uint8_t*)"BootLoad",8));
  assert(!proto_local_is_boot_request(NULL,8));
  assert(!proto_local_is_boot_request((const uint8_t*)"BootLoad",7));
  assert(!proto_local_is_boot_request((const uint8_t*)"BootLoad\r\n",10));
  assert(!proto_local_is_boot_request((const uint8_t*)"bootload",8));
  assert(!proto_local_is_boot_request((const uint8_t*)"xBootLoad",9));break;
 case 15:
  assert(!g_test_boot_writes&&!svc_boot_ready());
  assert(!svc_boot_get_status(NULL));break;
 case 16:
 {
  const uint8_t old=0xAA;g_test_boot_layout=1;
  assert(SVC_SERIAL_OK==svc_serial_send(COMM_PORT_4G,&old,1));
  tick(1);boot_command(COMM_PORT_RS485_1,10);
  g_test_boot_layout=0;complete(COMM_PORT_4G);tick(70);
  assert(!g_test_boot_writes&&!g_reset_count&&!app_local_busy());break;
 }
 case 17:
  g_test_boot_layout=1;
  assert(svc_gm800_start(COMM_PORT_RS485_1,"12345678901234567890",21));
  boot_command(COMM_PORT_RS485_2,60);
  assert(!g_test_boot_writes&&!g_reset_count);
  assert(svc_boot_get_status(&bs)&&!bs.requests);break;
 case 18:
  g_test_boot_layout=1;assert(app_mode_request(COMM_APP_MODE_HIK));
  boot_command(COMM_PORT_4G,60);
  assert(!g_test_boot_writes&&!g_reset_count);break;
 case 19:
  g_test_boot_layout=1;g_irq=1;
  boot_command(COMM_PORT_4G,100);
  assert(g_irq==1&&g_test_boot_writes==1&&g_reset_count==1);break;
 default:abort();
 }
 assert(!g_param_nv_writes&&!g_relay_nv_writes);
 return 0;
}
"""

STORAGE_HARNESS = r"""
#include <windows.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "stm32g0xx_hal.h"
#include "bsp_boot.h"
#include "boot_config.h"
static uint32_t image_base=BOOT_APP_ADDRESS,image_limit=BOOT_APP_LIMIT,vector_base=BOOT_APP_ADDRESS;
static unsigned unlocks,locks,erases,programs;
static int fail_unlock,fail_erase,fail_program,fail_verify,fail_lock;
uint32_t bsp_system_image_base(void){return image_base;}
uint32_t bsp_system_image_limit(void){return image_limit;}
uint32_t bsp_system_vector_base(void){return vector_base;}
HAL_StatusTypeDef HAL_FLASH_Unlock(void){unlocks++;return fail_unlock?HAL_ERROR:HAL_OK;}
HAL_StatusTypeDef HAL_FLASH_Lock(void){locks++;return fail_lock?HAL_ERROR:HAL_OK;}
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *p,uint32_t *e)
{
 assert(p->TypeErase==FLASH_TYPEERASE_PAGES&&p->Banks==FLASH_BANK_1);
 assert(p->Page==6&&p->NbPages==1);erases++;*e=0;
 if(fail_erase)return HAL_ERROR;
 memset((void*)(uintptr_t)BOOT_FLAG_ADDRESS,0xFF,2048);return HAL_OK;
}
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type,uint32_t address,uint64_t word)
{
 assert(type==FLASH_TYPEPROGRAM_DOUBLEWORD&&address==BOOT_FLAG_ADDRESS);
 assert(word==BOOT_FLAG_REQUEST||word==BOOT_FLAG_NORMAL);programs++;
 if(fail_program)return HAL_ERROR;
 *(uint64_t*)(uintptr_t)address=fail_verify?0x1234:word;return HAL_OK;
}
int main(int argc,char **argv)
{
 unsigned n=argc>1?(unsigned)atoi(argv[1]):0;uint64_t word;
 void *p=VirtualAlloc((void*)(uintptr_t)FLASH_BASE,131072,
                       MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 assert(p==(void*)(uintptr_t)FLASH_BASE);memset(p,0xA5,131072);
 switch(n)
 {
 case 0:
  assert(bsp_boot_layout_valid()&&bsp_boot_write_flag(0));
  assert(bsp_boot_read_flag(&word)&&word==0);
  assert(bsp_boot_write_flag(BOOT_FLAG_NORMAL));
  assert(bsp_boot_read_flag(&word)&&word==BOOT_FLAG_NORMAL);
  assert(erases==2&&programs==2&&locks==2);break;
 case 1:image_base=FLASH_BASE;vector_base=FLASH_BASE;assert(!bsp_boot_write_flag(0));break;
 case 2:vector_base=FLASH_BASE;assert(!bsp_boot_write_flag(0));break;
 case 3:image_limit=BOOT_APP_LIMIT+1;assert(!bsp_boot_write_flag(0));break;
 case 4:image_limit=image_base;assert(!bsp_boot_write_flag(0));break;
 case 5:assert(!bsp_boot_read_flag(NULL)&&!bsp_boot_write_flag(1));break;
 case 6:fail_unlock=1;assert(!bsp_boot_write_flag(0));assert(!erases&&!programs);break;
 case 7:fail_erase=1;assert(!bsp_boot_write_flag(0));assert(locks==1&&!programs);break;
 case 8:fail_program=1;assert(!bsp_boot_write_flag(0));assert(locks==1);break;
 case 9:fail_verify=1;assert(!bsp_boot_write_flag(0));assert(locks==1);break;
 case 10:fail_lock=1;assert(!bsp_boot_write_flag(0));assert(locks==1);break;
 case 11:
  assert(bsp_boot_write_flag(0)&&bsp_boot_write_flag(0));
  assert(erases==1&&programs==1&&unlocks==1);break;
 case 12:image_base=BOOT_APP_ADDRESS+2048;assert(!bsp_boot_read_flag(&word));break;
 default:abort();
 }
 if((n>=1&&n<=5)||n==12)assert(!unlocks&&!erases&&!programs);
 /* The loader, APP, parameters, unused backup and relay memory never changed. */
 assert(*(uint64_t*)(uintptr_t)0x08002800==UINT64_C(0xA5A5A5A5A5A5A5A5));
 assert(*(uint64_t*)(uintptr_t)0x08003800==UINT64_C(0xA5A5A5A5A5A5A5A5));
 assert(*(uint64_t*)(uintptr_t)0x0801E000==UINT64_C(0xA5A5A5A5A5A5A5A5));
 assert(*(uint64_t*)(uintptr_t)0x0801E800==UINT64_C(0xA5A5A5A5A5A5A5A5));
 assert(*(uint64_t*)(uintptr_t)0x0801F000==UINT64_C(0xA5A5A5A5A5A5A5A5));
 assert(VirtualFree(p,0,MEM_RELEASE));return 0;
}
"""

class BootTests(unittest.TestCase):
    def test_application(self):
        compiler=shutil.which('gcc')
        if not compiler: raise RuntimeError('Native GCC required')
        names=['DTO BootLoad UART1','DTO BootLoad UART2','DTO BootLoad UART3',
               'HIK BootLoad UART1','HIK BootLoad UART2','HIK BootLoad UART3',
               'unsafe direct-address layout consumes command without erase',
               'all three TX complete before flag write, no ACK',
               'any pending-port TX failure cancels upgrade',
               'failed flag write resumes RX, no retry, external retry works',
               'failed readback never resets or retries',
               'relocated startup marks 55BB once', 'normal marker avoids erase',
               'startup failure recorded without reset loop',
               'strict ASCII size/case and null bounds',
               'direct startup never writes and status guards',
               'layout rechecked at commit boundary',
               'module configuration excludes BootLoad',
               'mode transition excludes BootLoad',
               'maintenance preserves previous interrupt mask']
        with tempfile.TemporaryDirectory(prefix='comm-convert-boot-') as tmp:
            directory=pathlib.Path(tmp)
            (directory/'usart.h').write_text(HAL_HEADER,encoding='utf-8')
            source=HARNESS.replace('int main(int argc, char **argv)',
                                   'int serial_base_main(int argc, char **argv)')
            source=source.replace('static uint32_t g_reset_count;',
                                  'static uint32_t g_reset_count;\nstatic unsigned g_param_nv_writes,g_mode_flash_fail;')
            source=source.replace('memcpy(g_nv_data, p_words, 24U);',
                                  'g_param_nv_writes++;memcpy(g_nv_data, p_words, 24U);')
            source+=DTO_HARNESS.replace('int main(int argc, char **argv)',
                                        'int dto_base_main(int argc, char **argv)')
            source+=POLL_HARNESS.replace('int main(int argc,char **argv)',
                                         'int poll_base_main(int argc,char **argv)')
            source+=MODE_HARNESS.replace('int main(int argc,char **argv)',
                                         'int mode_base_main(int argc,char **argv)')
            source+=UNIFIED_HARNESS.replace('int main(int argc,char **argv)',
                                            'int unified_base_main(int argc,char **argv)')
            harness=directory/'harness.c'
            harness.write_text(source+BOOT_HARNESS,encoding='utf-8')
            exe=directory/'boot.exe'
            command=[compiler,'-std=c99','-Wall','-Wextra','-Werror']
            for inc in (directory,*(ROOT/p for p in ['Config','Bsp','Services','App','Protocols'])):
                command+=['-I',str(inc)]
            paths=['Bsp/bsp_uart.c','Services/svc_serial.c','Protocols/proto_gm800.c',
                   'Services/svc_gm800.c','Services/svc_params.c','Services/svc_boot.c',
                   'Config/comm_params.c','App/app_dto.c','App/app_main.c','App/app_local.c',
                   'App/app_mode.c','Protocols/proto_local.c','Services/svc_identity.c',
                   'Services/svc_relay.c','App/app_hik.c','Protocols/proto_hik.c',
                   'Services/svc_telemetry.c','Services/svc_poll.c','Protocols/proto_controller.c']
            command += [str(ROOT/p) for p in paths]+[str(harness),'-o',str(exe)]
            subprocess.run(command,check=True,timeout=60)
            for n,name in enumerate(names):
                with self.subTest(scenario=name):
                    subprocess.run([str(exe),str(n)],check=True,timeout=10)
                    print('PASS:',name)

    def test_storage(self):
        compiler=shutil.which('gcc')
        if not compiler: raise RuntimeError('Native GCC required')
        names=['dedicated page6, zero/55BB readback, other pages preserved',
               'direct image rejected before unlock','wrong VTOR rejected',
               'oversize image rejected','empty image rejected','null and flag bounds',
               'unlock failure','erase failure locks','program failure locks',
               'readback mismatch locks','lock failure','identical flag skips erase',
               'wrong relocated origin rejected']
        with tempfile.TemporaryDirectory(prefix='comm-convert-boot-flash-') as tmp:
            d=pathlib.Path(tmp)
            (d/'stm32g0xx_hal.h').write_text(FLASH_HEADER,encoding='utf-8')
            (d/'harness.c').write_text(STORAGE_HARNESS,encoding='utf-8')
            exe=d/'flash.exe'
            cmd=[compiler,'-std=c99','-Wall','-Wextra','-Werror','-Wno-int-to-pointer-cast']
            for inc in [d,ROOT/'Bsp',ROOT/'Config']:cmd+=['-I',str(inc)]
            cmd += [str(ROOT/'Bsp/bsp_boot.c'),str(d/'harness.c'),'-o',str(exe)]
            subprocess.run(cmd,check=True,timeout=60)
            for n,name in enumerate(names):
                with self.subTest(scenario=name):
                    subprocess.run([str(exe),str(n)],check=True,timeout=10)
                    print('PASS:',name)

if __name__=='__main__': unittest.main(verbosity=2)
