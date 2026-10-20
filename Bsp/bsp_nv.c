/**
 ******************************************************************************
 *@file               :   bsp_nv.c
 *@brief              :   Read and commit fixed parameter records in reserved Flash.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "stm32g0xx_hal.h"
#include "bsp_nv.h"
#include "params_config.h"
#include "dto_config.h"
/* define   -----------------------------------------------------------------*/
#if FLASH_PAGE_SIZE != PARAMS_PAGE_SIZE
#error "Parameter layout does not match device Flash pages"
#endif

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* private  functions  ------------------------------------------------------*/
static uint8_t
write_record(uint32_t address, const uint64_t *p_words, uint16_t count)
{
    FLASH_EraseInitTypeDef erase      = {0};
    uint32_t               page_error = 0U;
    uint32_t               index;
    uint8_t                success = 1U;
    if(HAL_OK != HAL_FLASH_Unlock())
    {
        return 0U;
    }
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Banks     = FLASH_BANK_1;
    erase.Page      = (address - FLASH_BASE) / FLASH_PAGE_SIZE;
    erase.NbPages   = 1U;
    if(HAL_OK != HAL_FLASHEx_Erase(&erase, &page_error))
    {
        success = 0U;
    }
    for(index = 0U; 0U != success && index < count; index++)
    {
        uint32_t word   = index;
        uint32_t target = address + word * 8U;
        if(HAL_OK != HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, target,
                                       p_words[word]) ||
           *(volatile const uint64_t *)target != p_words[word])
        {
            success = 0U;
        }
    }
    if(HAL_OK != HAL_FLASH_Lock())
    {
        success = 0U;
    }
    return success;
}

/* exported functions -------------------------------------------------------*/
uint8_t bsp_nv_read(uint64_t *p_words, uint16_t count)
{
    uint32_t index;
    if(NULL == p_words || PARAMS_RECORD_WORDS != count)
    {
        return 0U;
    }
    for(index = 0U; index < PARAMS_RECORD_WORDS; index++)
    {
        p_words[index] = *(volatile const uint64_t *)(PARAMS_ADDRESS +
                                                      index * 8U);
    }
    return 1U;
}
uint8_t bsp_nv_write(const uint64_t *p_words, uint16_t count)
{
    if(NULL == p_words || PARAMS_RECORD_WORDS != count)
    {
        return 0U;
    }
    return write_record(PARAMS_ADDRESS, p_words, count);
}
uint8_t bsp_nv_read_relays(uint64_t *p_word)
{
    if(NULL == p_word)
    {
        return 0U;
    }
    *p_word = *(volatile const uint64_t *)DTO_RELAY_ADDRESS;
    return 1U;
}
uint8_t bsp_nv_write_relays(uint64_t word)
{
    if(word > 3U)
    {
        return 0U;
    }
    return write_record(DTO_RELAY_ADDRESS, &word, 1U);
}

/* end of file --------------------------------------------------------------*/
