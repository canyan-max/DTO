
"""Single-page persistence tests with fake Flash and UART HAL."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

from test_serial import HAL_HEADER, HARNESS, ROOT
from test_dto import DTO_HARNESS


PARAM_HARNESS = r"""
#include "svc_params.h"
#include "svc_gm800.h"
#include "params_config.h"
static svc_params_status_t param_status(void)
{
    svc_params_status_t status;
    assert(1U==svc_params_get_status(&status));
    return status;
}
static comm_params_t settings(void)
{
    comm_params_t result;
    assert(1U==svc_params_get(&result));
    return result;
}
static void make_record(uint32_t baud)
{
    uint32_t sum;
    g_nv_data[0U]=(uint64_t)baud | ((uint64_t)115200U<<32U);
    g_nv_data[1U]=(uint64_t)10000U | ((uint64_t)12000U<<16U);
    sum=(uint32_t)g_nv_data[0U]^(uint32_t)(g_nv_data[0U]>>32U)^
        (uint32_t)g_nv_data[1U]^(uint32_t)(g_nv_data[1U]>>32U);
    g_nv_data[2U]=(uint64_t)PARAMS_VALID_MAGIC | ((uint64_t)sum<<32U);
}
static void test_defaults(void)
{
    comm_params_t value=settings();
    assert(9600U==value.rs485_1_baud && 9600U==value.rs485_2_baud);
    assert(10000U==value.uv_protect_mv && 12000U==value.uv_recover_mv);
    assert(0U==value.uv_enabled);
    assert(SVC_PARAMS_DEFAULTS==param_status().source);
    assert(0U==g_nv_writes);
}
static void test_save_reload(void)
{
    comm_params_t value=settings();
    value.rs485_1_baud=19200U;
    value.rs485_2_baud=38400U;
    assert(1U==svc_params_set(&value));
    assert(1U==svc_params_save());
    assert(SVC_PARAMS_FLASH==param_status().source);
    assert(1U==g_nv_writes);
    svc_params_init();
    assert(19200U==settings().rs485_1_baud);
    assert(38400U==settings().rs485_2_baud);
    value.rs485_1_baud=57600U;
    assert(1U==svc_params_set(&value));
    assert(1U==svc_params_save());
    svc_params_init();
    assert(57600U==settings().rs485_1_baud);
    assert(2U==g_nv_writes);
    assert(9600U==huart2.Init.BaudRate);
}
static void test_legacy_boot(void)
{
    uint64_t before[3U];
    memcpy(before,g_nv_data,sizeof(before));
    assert(SVC_PARAMS_FLASH==param_status().source);
    assert(9600U==huart2.Init.BaudRate && 115200U==huart3.Init.BaudRate);
    assert(1U==svc_params_save());
    assert(0U==g_nv_writes);
    assert(0==memcmp(before,g_nv_data,sizeof(before)));
}
static void test_torn_write(void)
{
    int stage;
    for(stage=0;stage<3;stage++)
    {
        comm_params_t value;
        test_nv_reset();
        svc_params_init();
        value=settings();
        value.rs485_1_baud=19200U;
        assert(1U==svc_params_set(&value));
        assert(1U==svc_params_save());
        value.rs485_1_baud=38400U;
        assert(1U==svc_params_set(&value));
        g_nv_fail_step=stage;
        assert(0U==svc_params_save());
        g_nv_fail_step=-1;
        svc_params_init();
        assert(SVC_PARAMS_DEFAULTS==param_status().source);
        assert(9600U==settings().rs485_1_baud);
    }
}
static void test_corrupt_fallback(void)
{
    comm_params_t value=settings();
    value.rs485_1_baud=19200U;
    assert(1U==svc_params_set(&value));
    assert(1U==svc_params_save());
    g_nv_data[0U]^=1U;
    svc_params_init();
    assert(SVC_PARAMS_DEFAULTS==param_status().source);
    assert(9600U==settings().rs485_1_baud);
}
static void test_validation(void)
{
    comm_params_t value=settings();
    value.rs485_1_baud=0U;
    assert(0U==svc_params_set(&value));
    value.rs485_1_baud=12345U;
    assert(0U==svc_params_set(&value));
    value=settings();
    value.uv_enabled=2U;
    assert(0U==svc_params_set(&value));
    value=settings();
    value.uv_protect_mv=value.uv_recover_mv;
    assert(0U==svc_params_set(&value));
    assert(0U==svc_params_set(NULL));
    assert(0U==svc_params_get(NULL));
    assert(0U==svc_params_get_status(NULL));
    assert(0U==g_nv_writes);
}
static void test_reset_defaults(void)
{
    comm_params_t value=settings();
    value.rs485_1_baud=38400U;
    assert(1U==svc_params_set(&value));
    assert(1U==svc_params_save());
    assert(1U==svc_params_save_defaults());
    svc_params_init();
    assert(9600U==settings().rs485_1_baud);
    assert(SVC_PARAMS_FLASH==param_status().source);
    assert(2U==g_nv_writes);
}
static void test_write_guards(void)
{
    uint8_t value=1U;
    assert(SVC_SERIAL_OK==svc_serial_send(COMM_PORT_4G,&value,1U));
    assert(0U==svc_params_save());
    assert(0U==g_nv_writes);
    svc_serial_task();
    complete(COMM_PORT_4G);
    svc_serial_task();
    assert(1U==svc_gm800_start(COMM_PORT_RS485_1, "12345678901234567890",21U));
    assert(0U==svc_params_save_defaults());
    assert(0U==g_nv_writes);
}
static void test_unchanged(void)
{
    unsigned index;
    assert(1U==svc_params_save());
    for(index=0U;index<10U;index++)
    {
        assert(1U==svc_params_save());
    }
    assert(1U==g_nv_writes);
    assert(1U==param_status().saves);
}
static void test_invalid_saved_fields(void)
{
    make_record(12345U);
    svc_params_init();
    assert(SVC_PARAMS_DEFAULTS==param_status().source);
    make_record(9600U);
    g_nv_data[2U]^=1U;
    svc_params_init();
    assert(SVC_PARAMS_DEFAULTS==param_status().source);
}
int main(int argc,char **argv)
{
    int scenario;
    assert(2==argc);
    scenario=atoi(argv[1]);
    test_nv_reset();
    if(2==scenario) { make_record(9600U); }
    assert(1U==app_init());
    switch(scenario)
    {
        case 0: test_defaults(); break;
        case 1: test_save_reload(); break;
        case 2: test_legacy_boot(); break;
        case 3: test_torn_write(); break;
        case 4: test_corrupt_fallback(); break;
        case 5: test_validation(); break;
        case 6: test_reset_defaults(); break;
        case 7: test_write_guards(); break;
        case 8: test_unchanged(); break;
        case 9: test_invalid_saved_fields(); break;
        default: abort();
    }
    return 0;
}
"""
class ParamTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler=shutil.which("gcc")
        if compiler is None: raise RuntimeError("Native GCC required")
        cls.temp=tempfile.TemporaryDirectory(prefix="comm-convert-params-")
        cls.addClassCleanup(cls.temp.cleanup)
        directory=pathlib.Path(cls.temp.name)
        (directory/"usart.h").write_text(HAL_HEADER,encoding="utf-8")
        source=HARNESS.replace("int main(int argc, char **argv)",
                               "int serial_base_main(int argc, char **argv)")
        source=source.replace("static uint8_t g_nv_ready;",
                              """static uint8_t g_nv_ready;
static int g_nv_fail_step=-1;
static unsigned g_nv_writes;""")
        source=source.replace(
            "memcpy(g_nv_data, p_words, 24U);",
            """unsigned i;
    g_nv_writes++;
    memset(g_nv_data,0xFF,sizeof(g_nv_data));
    for(i=0U;i<3U;i++)
    {
        if((int)i==g_nv_fail_step) { return 0U; }
        g_nv_data[i]=p_words[i];
    }""")
        source+=DTO_HARNESS.replace("int main(int argc, char **argv)",
                                    "int dto_base_main(int argc, char **argv)")
        harness=directory/"params_harness.c"
        harness.write_text(source+PARAM_HARNESS,encoding="utf-8")
        cls.executable=directory/"params_test.exe"
        command=[compiler, '-DCOMM_APP_ENABLE_MODE=0',"-DCOMM_APP_ENABLE_DTO=1","-DCOMM_APP_ENABLE_HIK=0","-std=c99","-Wall","-Wextra","-Werror"]
        for include in (directory,ROOT/"Config",ROOT/"Bsp",ROOT/"Services",
                        ROOT/"App",ROOT/"Protocols"):
            command+=["-I",str(include)]
        for path in ("Bsp/bsp_uart.c","Services/svc_serial.c",
                     "Protocols/proto_gm800.c","Services/svc_gm800.c",
                     "Services/svc_params.c", "Services/svc_boot.c","Config/comm_params.c",
                     "App/app_dto.c","App/app_main.c",
                     "Services/svc_identity.c", "Services/svc_relay.c", "Services/svc_telemetry.c", "App/app_local.c","Protocols/proto_local.c"):
            command.append(str(ROOT/path))
        command += [str(harness),"-o",str(cls.executable)]
        subprocess.run(command,check=True,cwd=ROOT,timeout=60)
    def test_persistence(self):
        scenarios=[
            "empty page uses defaults without writing",
            "single-page save and reload",
            "legacy record restores baud without rewriting identical data",
            "interrupted overwrite falls back to defaults",
            "corrupt record uses defaults",
            "invalid settings rejected",
            "restore defaults overwrites the same page",
            "TX and GM800 activity prevent writes",
            "repeated identical saves do not erase Flash",
            "invalid saved rate or marker rejected",
        ]
        for index,name in enumerate(scenarios):
            with self.subTest(scenario=name):
                subprocess.run([str(self.executable),str(index)],check=True,
                               timeout=10,cwd=ROOT)
                print("PASS:",name)
if __name__=="__main__":
    unittest.main(verbosity=2)
