/**
 ******************************************************************************
 *@file               :   bsp_boot.c
 *@brief              :   Guard and commit the dedicated legacy boot flag page.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "stm32g0xx_hal.h"
#include "bsp_boot.h"
#include "bsp_system.h"
#include "boot_config.h"
#include "params_config.h"
/* define   -----------------------------------------------------------------*/
#if FLASH_PAGE_SIZE != BOOT_FLAG_PAGE_SIZE
#error "Boot flag layout does not match device Flash pages"
#endif
#if BOOT_FLAG_ADDRESS + BOOT_FLAG_PAGE_SIZE != BOOT_APP_ADDRESS
#error "Boot flag page must end at application origin"
#endif
#if BOOT_APP_LIMIT > PARAMS_ADDRESS || BOOT_FLAG_ADDRESS >= BOOT_APP_LIMIT
#error "Boot application layout overlaps parameter storage"
#endif

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* private  functions  ------------------------------------------------------*/

/* exported functions -------------------------------------------------------*/
uint8_t bsp_boot_layout_valid(void)
{
    uint32_t base  = bsp_system_image_base();
    uint32_t limit = bsp_system_image_limit();
    return BOOT_APP_ADDRESS == base && limit > base &&
           limit <= BOOT_APP_LIMIT &&
           BOOT_APP_ADDRESS == bsp_system_vector_base();
}
uint8_t bsp_boot_read_flag(uint64_t *p_word)
{
    if(NULL == p_word || 0U == bsp_boot_layout_valid())
    {
        return 0U;
    }
    *p_word = *(volatile const uint64_t *)BOOT_FLAG_ADDRESS;
    return 1U;
}
uint8_t bsp_boot_write_flag(uint16_t flag)
{
    FLASH_EraseInitTypeDef erase      = {0};
    uint32_t               page_error = 0U;
    uint8_t                success    = 1U;
    uint64_t               word       = flag;
    if((BOOT_FLAG_NORMAL != flag && BOOT_FLAG_REQUEST != flag) ||
       0U == bsp_boot_layout_valid())
    {
        return 0U;
    }
    if(word == *(volatile const uint64_t *)BOOT_FLAG_ADDRESS)
    {
        return 1U;
    }
    if(HAL_OK != HAL_FLASH_Unlock())
    {
        return 0U;
    }
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Banks     = FLASH_BANK_1;
    erase.Page      = (BOOT_FLAG_ADDRESS - FLASH_BASE) / FLASH_PAGE_SIZE;
    erase.NbPages   = 1U;
    if(HAL_OK != HAL_FLASHEx_Erase(&erase, &page_error))
    {
        success = 0U;
    }
    if(0U != success &&
       (HAL_OK != HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                                    BOOT_FLAG_ADDRESS, word) ||
        word != *(volatile const uint64_t *)BOOT_FLAG_ADDRESS))
    {
        success = 0U;
    }
    if(HAL_OK != HAL_FLASH_Lock())
    {
        success = 0U;
    }
    return success;
}

/* end of file --------------------------------------------------------------*/
