"""Actual ADC BSP and sampling service against a controlled polling HAL."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]

HEADER = r'''
#ifndef TEST_ADC_H
#define TEST_ADC_H
#include <stdint.h>
#include <stddef.h>
#define DISABLE 0U
#define ADC_RESOLUTION_12B 12U
#define ADC_DATAALIGN_RIGHT 0U
#define ADC_SCAN_DISABLE 0U
#define ADC_SOFTWARE_START 0U
#define ADC_EOC_SINGLE_CONV 0U
#define ADC_FLAG_EOC 1U
#define ADC_FLAG_OVR 2U
typedef enum {HAL_OK,HAL_ERROR} HAL_StatusTypeDef;
typedef struct {
 uint32_t Resolution,DataAlign,ScanConvMode,ExternalTrigConv,EOCSelection;
 uint32_t NbrOfConversion,ContinuousConvMode;
} ADC_InitTypeDef;
typedef struct {ADC_InitTypeDef Init; void *DMA_Handle;} ADC_HandleTypeDef;
extern ADC_HandleTypeDef hadc1;
extern uint32_t flags;
#define __HAL_ADC_GET_FLAG(h,f) (flags & (f))
HAL_StatusTypeDef HAL_ADC_Init(ADC_HandleTypeDef *h);
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *h);
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *h);
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef *h);
HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef *h,uint32_t t);
uint32_t HAL_ADC_GetValue(const ADC_HandleTypeDef *h);
#endif
'''

HARNESS = r'''
#include <assert.h>
#include <stdlib.h>
#include "adc.h"
#include "bsp_adc.h"
#include "svc_sampling.h"
#include "sampling_config.h"
ADC_HandleTypeDef hadc1;
uint32_t flags;
static uint32_t now_ms,value,starts,polls,stops;
static int fail_init,fail_cal,fail_start,fail_poll,fail_stop;
uint32_t bsp_system_time_ms(void) {return now_ms;}
HAL_StatusTypeDef HAL_ADC_Init(ADC_HandleTypeDef *h)
{assert(0U==h->Init.ContinuousConvMode);return fail_init?HAL_ERROR:HAL_OK;}
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *h)
{assert(h==&hadc1);return fail_cal?HAL_ERROR:HAL_OK;}
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *h)
{assert(h==&hadc1);starts++;flags=0U;return fail_start?HAL_ERROR:HAL_OK;}
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef *h)
{assert(h==&hadc1);stops++;return fail_stop?HAL_ERROR:HAL_OK;}
HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef *h,uint32_t t)
{assert(h==&hadc1 && t==0U && (flags&ADC_FLAG_EOC));polls++;
 return fail_poll?HAL_ERROR:HAL_OK;}
uint32_t HAL_ADC_GetValue(const ADC_HandleTypeDef *h)
{assert(h==&hadc1);flags=0U;return value;}
static svc_sampling_status_t status(void)
{svc_sampling_status_t s;assert(svc_sampling_get_status(&s));return s;}
static void sample(uint32_t raw)
{now_ms+=10U;svc_sampling_task();assert(status().active);
 value=raw;flags=ADC_FLAG_EOC;svc_sampling_task();assert(!status().active);}
static void block(uint32_t raw)
{unsigned i;for(i=0;i<16;i++)sample(raw);}
int main(int argc,char **argv)
{
 int scenario=argc>1?atoi(argv[1]):0;
 hadc1.Init.Resolution=ADC_RESOLUTION_12B;
 hadc1.Init.NbrOfConversion=1U;
 hadc1.Init.ContinuousConvMode=1U;
 if(scenario==5) fail_cal=1;
 if(scenario==8) now_ms=0xFFFFFFA0U;
 svc_sampling_init();
 switch(scenario)
 {
 case 0:
  assert(status().initialized && !status().valid);
  now_ms=9U;svc_sampling_task();assert(starts==0U);
  now_ms=10U;svc_sampling_task();assert(starts==1U);
  svc_sampling_task();assert(polls==0U && status().active);
  value=1234U;flags=ADC_FLAG_EOC;svc_sampling_task();
  assert(status().raw==1234U && status().samples==1U && !status().valid);
  assert(!svc_sampling_get_status(NULL));break;
 case 1:
  {unsigned i;for(i=0;i<15;i++)sample(1000U);
   assert(!status().valid);sample(1016U);
   assert(status().filtered_raw==1001U && status().updates==1U);
   block(2000U);assert(status().filtered_raw==2000U && status().updates==2U);}
  break;
 case 2:
  block(0U);assert(status().input_mv==0U);
  block(4095U);assert(status().input_mv==47300U && status().adc_pin_mv==3300U);
  block(1040U);assert(status().input_mv==(1040U*3300U/4095U)*107500U/7500U);
  break;
 case 3:
  block(1040U);now_ms+=10U;svc_sampling_task();now_ms+=5U;
  svc_sampling_task();assert(status().timeouts==1U && status().errors==1U);
  assert(!status().initialized && !status().valid && !status().active);
  now_ms+=999U;svc_sampling_task();assert(!status().initialized);
  now_ms++;svc_sampling_task();assert(status().initialized);
  block(2000U);assert(status().valid && status().filtered_raw==2000U);break;
 case 4:
  block(1000U);now_ms+=1000U;assert(!status().valid);
  svc_sampling_task();value=2000U;flags=ADC_FLAG_EOC;svc_sampling_task();
  assert(!status().valid);{unsigned i;for(i=0;i<15;i++)sample(2000U);}
  assert(status().valid && status().filtered_raw==2000U);break;
 case 5:
  assert(!status().initialized && status().errors==1U);
  fail_cal=0;now_ms=1000U;svc_sampling_task();assert(status().initialized);
  fail_start=1;now_ms+=10U;svc_sampling_task();
  assert(!status().initialized && status().errors==2U);break;
 case 6:
  now_ms+=10U;svc_sampling_task();flags=ADC_FLAG_OVR|ADC_FLAG_EOC;
  svc_sampling_task();assert(status().errors==1U && status().samples==0U);
  break;
 case 7:
  now_ms+=10U;svc_sampling_task();value=4096U;flags=ADC_FLAG_EOC;
  svc_sampling_task();assert(status().errors==1U && status().samples==0U);
  break;
 case 8:
  block(1234U);assert(status().valid && status().filtered_raw==1234U);
  assert(status().samples==16U);break;
 case 9:
  now_ms+=10U;svc_sampling_task();fail_stop=1;flags=ADC_FLAG_EOC;
  svc_sampling_task();assert(status().active && !status().initialized);
  fail_stop=0;now_ms+=1000U;svc_sampling_task();
  assert(status().initialized && !status().active);break;
 case 10:
  now_ms+=10U;svc_sampling_task();fail_poll=1;flags=ADC_FLAG_EOC;
  svc_sampling_task();assert(status().errors==1U && !status().valid);break;
 case 11:
  assert(!bsp_adc_start() || status().initialized);
  assert(!bsp_adc_start());assert(bsp_adc_stop());
  assert(BSP_ADC_ERROR==bsp_adc_read(NULL));
  hadc1.DMA_Handle=&value;assert(!bsp_adc_init());break;
 default: assert(0);
 }
 return 0;
}
'''


class SamplingTests(unittest.TestCase):
    def test_sampling(self):
        compiler = shutil.which('gcc')
        if compiler is None:
            raise RuntimeError('Native GCC required')
        names = ['schedule and nonblocking EOC', 'complete block average',
                 'Program scaling and full range', 'timeout and bounded retry',
                 'stale data invalidation', 'calibration and start failure',
                 'overrun rejected', 'out of range rejected', 'tick rollover',
                 'stop failure recovery', 'HAL poll failure', 'BSP guards']
        with tempfile.TemporaryDirectory(prefix='comm-convert-adc-') as temporary:
            directory = pathlib.Path(temporary)
            (directory / 'adc.h').write_text(HEADER, encoding='utf-8')
            harness = directory / 'harness.c'
            harness.write_text(HARNESS, encoding='utf-8')
            executable = directory / 'sampling.exe'
            command = [compiler, '-std=c99', '-Wall', '-Wextra', '-Werror']
            for include in (directory, ROOT/'Bsp', ROOT/'Services', ROOT/'Config'):
                command += ['-I', str(include)]
            command += [str(ROOT/'Bsp/bsp_adc.c'),
                        str(ROOT/'Services/svc_sampling.c'), str(harness),
                        '-o', str(executable)]
            subprocess.run(command, check=True, timeout=60)
            for index, name in enumerate(names):
                with self.subTest(scenario=name):
                    subprocess.run([str(executable), str(index)],
                                   check=True, timeout=10)
                    print('PASS:', name)


if __name__ == '__main__':
    unittest.main(verbosity=2)
