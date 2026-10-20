"""Actual GPIO/Flash BSP with fake HAL and mapped host RAM, never device Flash."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT=pathlib.Path(__file__).resolve().parents[2]
HEADER=r'''
#ifndef TEST_HAL_H
#define TEST_HAL_H
#include <stdint.h>
#include <stddef.h>
#define FLASH_PAGE_SIZE 2048U
#define FLASH_BASE 0x08000000U
#define FLASH_BANK_1 1U
#define FLASH_TYPEERASE_PAGES 1U
#define FLASH_TYPEPROGRAM_DOUBLEWORD 2U
typedef enum {HAL_OK,HAL_ERROR} HAL_StatusTypeDef;
typedef struct {uint32_t TypeErase,Banks,Page,NbPages;} FLASH_EraseInitTypeDef;
typedef struct {uint32_t id;} GPIO_TypeDef;
typedef enum {GPIO_PIN_RESET,GPIO_PIN_SET} GPIO_PinState;
extern GPIO_TypeDef test_port;
#define G2ROUT1_GPIO_Port (&test_port)
#define G2ROUT2_GPIO_Port (&test_port)
#define G2ROUT3_GPIO_Port (&test_port)
#define G2ROUT4_GPIO_Port (&test_port)
#define GM800FVDDEN_GPIO_Port (&test_port)
#define VCCEN1_GPIO_Port (&test_port)
#define VCCEN2_GPIO_Port (&test_port)
#define LED1_GPIO_Port (&test_port)
#define LED2_GPIO_Port (&test_port)
#define LED3_GPIO_Port (&test_port)
#define G2ROUT1_Pin 1U
#define G2ROUT2_Pin 2U
#define G2ROUT3_Pin 4U
#define G2ROUT4_Pin 8U
#define GM800FVDDEN_Pin 16U
#define VCCEN1_Pin 32U
#define VCCEN2_Pin 64U
#define LED1_Pin 128U
#define LED2_Pin 256U
#define LED3_Pin 512U
void HAL_GPIO_WritePin(GPIO_TypeDef *p,uint16_t pin,GPIO_PinState value);
HAL_StatusTypeDef HAL_FLASH_Unlock(void);
HAL_StatusTypeDef HAL_FLASH_Lock(void);
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *p,uint32_t *e);
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type,uint32_t addr,uint64_t data);
#endif
'''
HARNESS=r'''
#include <windows.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "stm32g0xx_hal.h"
#include "bsp_nv.h"
#include "bsp_io.h"
#include "params_config.h"
#include "dto_config.h"
GPIO_TypeDef test_port;
static unsigned erases,programs,locks,pins;
static uint32_t last_page;
static GPIO_PinState last_level;
static int fail_erase,fail_program,fail_lock;
void HAL_GPIO_WritePin(GPIO_TypeDef *p,uint16_t pin,GPIO_PinState value)
{assert(p==&test_port);assert(pin!=4U&&pin!=8U);pins=pin;last_level=value;}
HAL_StatusTypeDef HAL_FLASH_Unlock(void) {return HAL_OK;}
HAL_StatusTypeDef HAL_FLASH_Lock(void) {locks++;return fail_lock?HAL_ERROR:HAL_OK;}
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *p,uint32_t *e)
{
 assert(p->TypeErase==FLASH_TYPEERASE_PAGES&&p->Banks==FLASH_BANK_1);
 assert(p->NbPages==1U);assert(p->Page==60U||p->Page==62U);
 erases++;last_page=p->Page;*e=0U;
 if(fail_erase)return HAL_ERROR;
 memset((void *)(uintptr_t)(FLASH_BASE+p->Page*2048U),0xFF,2048U);return HAL_OK;
}
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type,uint32_t addr,uint64_t data)
{
 assert(type==FLASH_TYPEPROGRAM_DOUBLEWORD);programs++;
 assert((addr>=PARAMS_ADDRESS&&addr<PARAMS_ADDRESS+24U)||addr==DTO_RELAY_ADDRESS);
 if(fail_program)return HAL_ERROR;
 *(uint64_t *)(uintptr_t)addr=data;return HAL_OK;
}
int main(int argc,char **argv)
{
 uint64_t words[3U]={0x1234U,0x5678U,0x9ABCU},read[3U];
 uint64_t relay;
 int n=argc>1?atoi(argv[1]):0;
 void *mapped=VirtualAlloc((void *)(uintptr_t)FLASH_BASE,131072U,
                           MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 assert(mapped==(void *)(uintptr_t)FLASH_BASE);
 memset(mapped,0xFF,131072U);
 switch(n)
 {
 case 0:
  assert(bsp_nv_write(words,3U)&&last_page==60U&&programs==3U&&locks==1U);
  assert(bsp_nv_read(read,3U)&&!memcmp(words,read,sizeof(words)));
  assert(bsp_nv_write_relays(2U)&&last_page==62U);
  assert(bsp_nv_read_relays(&relay)&&relay==2U);
  assert(bsp_nv_read(read,3U)&&!memcmp(words,read,sizeof(words)));
  assert(*(uint64_t *)(uintptr_t)0x0801E800U==UINT64_MAX);break;
 case 1:
  fail_erase=1;assert(!bsp_nv_write_relays(1U));
  assert(erases==1U&&!programs&&locks==1U);break;
 case 2:
  fail_program=1;assert(!bsp_nv_write_relays(1U));assert(locks==1U);break;
 case 3:
  fail_lock=1;assert(!bsp_nv_write_relays(1U));assert(locks==1U);break;
 case 4:
  assert(!bsp_nv_write_relays(4U)&&!bsp_nv_read_relays(NULL));
  assert(!bsp_nv_read(NULL,3U)&&!bsp_nv_write(words,2U));
  assert(!erases&&!programs);break;
 case 5:
  assert(bsp_io_set_relay(0U,0U)&&pins==1U&&last_level==GPIO_PIN_RESET);
  assert(bsp_io_set_relay(1U,1U)&&pins==2U&&last_level==GPIO_PIN_SET);
  assert(!bsp_io_set_relay(2U,1U)&&!bsp_io_set_relay(3U,0U));
  assert(!bsp_io_set_relay(0U,2U));
  assert(bsp_io_set_led(0U,1U)&&pins==128U&&last_level==GPIO_PIN_RESET);
  assert(bsp_io_set_led(2U,0U)&&pins==512U&&last_level==GPIO_PIN_SET);
  assert(!bsp_io_set_led(3U,1U));break;
 default:assert(0);
 }
 assert(VirtualFree(mapped,0U,MEM_RELEASE));return 0;
}
'''


class BoardTests(unittest.TestCase):
    def test_board(self):
        compiler=shutil.which('gcc')
        if compiler is None:
            raise RuntimeError('Native GCC required')
        names=['separate pages, readback and no backup-page access',
               'erase failure locks','program failure locks','lock failure',
               'fixed-layout guards','only two GPIO relays and LED polarity']
        with tempfile.TemporaryDirectory(prefix='comm-convert-board-') as tmp:
            directory=pathlib.Path(tmp)
            (directory/'stm32g0xx_hal.h').write_text(HEADER,encoding='utf-8')
            (directory/'main.h').write_text('#include "stm32g0xx_hal.h"\n',encoding='utf-8')
            harness=directory/'harness.c'
            harness.write_text(HARNESS,encoding='utf-8')
            executable=directory/'board.exe'
            command=[compiler,'-std=c99','-Wall','-Wextra','-Werror',
                     '-Wno-int-to-pointer-cast']
            for include in (directory,ROOT/'Bsp',ROOT/'Config'):
                command+=['-I',str(include)]
            command += [str(ROOT/'Bsp/bsp_nv.c'),str(ROOT/'Bsp/bsp_io.c'),
                        str(harness),'-o',str(executable)]
            subprocess.run(command,check=True,timeout=60)
            for index,name in enumerate(names):
                with self.subTest(scenario=name):
                    subprocess.run([str(executable),str(index)],check=True,timeout=10)
                    print('PASS:',name)


if __name__=='__main__':
    unittest.main(verbosity=2)
