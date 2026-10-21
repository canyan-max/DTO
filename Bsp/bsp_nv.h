/**
 ******************************************************************************
 *@file               :   bsp_nv.h
 *@brief              :   Read and commit fixed parameter records in reserved Flash.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef BSP_NV_H
#define BSP_NV_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
/* Relay page: legacy logical bits or tagged requested GPIO bits; one word.
 */
uint8_t bsp_nv_read_relays(uint64_t *p_word);
uint8_t bsp_nv_write_relays(uint64_t word);
/* One fixed page; p_words contains three legacy-format doublewords. */
uint8_t bsp_nv_read(uint64_t *p_words, uint16_t count);
/* Main only, quiescent communication maintenance window required.
 * Erases only PARAMS_ADDRESS. Word 2 is the final commit marker. */
uint8_t bsp_nv_write(const uint64_t *p_words, uint16_t count);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* BSP_NV_H */
/* end of file --------------------------------------------------------------*/
