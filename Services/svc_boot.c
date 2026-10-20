/**
 ******************************************************************************
 *@file               :   svc_boot.c
 *@brief              :   Legacy application marker and explicit upgrade flag requests.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include <string.h>
#include "svc_boot.h"
#include "bsp_boot.h"
#include "boot_config.h"
#include "svc_gm800.h"
#include "svc_serial.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/
static svc_boot_status_t g_boot_status;

/* private  functions  ------------------------------------------------------*/

/* exported functions -------------------------------------------------------*/
void svc_boot_init(void)
{
    uint64_t word;
    memset(&g_boot_status, 0, sizeof(g_boot_status));
    if(0U == bsp_boot_layout_valid())
    {
        return;
    }
    if(0U == bsp_boot_read_flag(&word))
    {
        g_boot_status.failures++;
        return;
    }
    /* Match the old application's low-32-bit normal marker check. */
    if(BOOT_FLAG_NORMAL != (uint32_t)word)
    {
        if(0U == bsp_boot_write_flag(BOOT_FLAG_NORMAL))
        {
            g_boot_status.failures++;
            return;
        }
        g_boot_status.startup_marks++;
    }
}
uint8_t svc_boot_ready(void)
{
    if(0U == bsp_boot_layout_valid())
    {
        g_boot_status.layout_rejects++;
        return 0U;
    }
    return 1U;
}
uint8_t svc_boot_request(void)
{
    uint32_t index;
    uint16_t pending;
    uint64_t word;
    g_boot_status.requests++;
    if(0U == svc_boot_ready() || 0U != svc_gm800_busy())
    {
        g_boot_status.failures++;
        return 0U;
    }
    for(index = 0U; index < COMM_PORT_NUM; index++)
    {
        if(0U == svc_serial_tx_pending((comm_port_t)index, &pending) ||
           0U != pending)
        {
            g_boot_status.failures++;
            return 0U;
        }
    }
    if(0U == bsp_boot_write_flag(BOOT_FLAG_REQUEST) ||
       0U == bsp_boot_read_flag(&word) || BOOT_FLAG_REQUEST != word)
    {
        g_boot_status.failures++;
        return 0U;
    }
    return 1U;
}
uint8_t svc_boot_get_status(svc_boot_status_t *p_status)
{
    if(NULL == p_status)
    {
        return 0U;
    }
    *p_status = g_boot_status;
    return 1U;
}

/* end of file --------------------------------------------------------------*/
