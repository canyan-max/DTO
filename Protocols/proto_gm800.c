/**
 ******************************************************************************
 *@file               :   proto_gm800.c
 *@brief              :   Bounded legacy CLOUD request parsing and AT command encoding.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include <string.h>
#include "proto_gm800.h"
#include "gm800_config.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/
static const char *const p_commands[] = {
    "+++",
    "a",
    "AT+WKMOD=NET\r\n",
    "AT+SOCKAEN=ON\r\n",
    "AT+SOCKA=TCP,62195.cnsh.iot-tcp.com,15000\r\n",
    "AT+SOCKASL=LONG\r\n",
    "AT+SHORTATM=10\r\n",
    "AT+SOCKRSTIM=10\r\n",
    "AT+SOCKRSNUM=60\r\n",
    "AT+HEARTEN=ON\r\n",
    "AT+HEARTTM=30\r\n",
    "AT+HEARTTP=NET\r\n",
    "AT+HEARTSORT=USER\r\n",
    "AT+HEARTDT=7777772E7573722E636E\r\n",
    "AT+REGEN=ON\r\n",
    "AT+REGSND=LINK\r\n",
    "AT+REGTP=CLOUD\r\n",
    NULL,
    "AT+ICCID?\r\n",
    "AT+IMEI?\r\n",
    "AT+S\r\n",
    "AT+S\r\n",
    "AT+Z\r\n",
};

/* private  functions  ------------------------------------------------------*/

/* exported functions -------------------------------------------------------*/

void proto_gm800_response_reset(proto_gm800_response_t *p_response)
{
    if(NULL != p_response)
    {
        memset(p_response, 0, sizeof(*p_response));
    }
}
uint8_t proto_gm800_response_feed(proto_gm800_response_t *p_response,
                                  uint8_t                 step,
                                  const uint8_t          *p_data,
                                  uint16_t                length)
{
    uint16_t index;
    uint8_t  result = 0U;
    if(NULL == p_response || NULL == p_data ||
       step >= PROTO_GM800_COMMAND_COUNT)
    {
        return 0U;
    }
    for(index = 0U; index < length; index++)
    {
        uint8_t value = p_data[index];
        if(0U == step && 0U == p_response->length && 'a' == value)
        {
            result |= PROTO_GM800_RESPONSE_OK;
            continue;
        }
        if('\r' == value || '\n' == value)
        {
            if(0U == p_response->overflow)
            {
                p_response->line[p_response->length] = '\0';
                if(0 == strcmp(p_response->line, "ERROR") ||
                   0 == strcmp(p_response->line, "ERR") ||
                   0 == strncmp(p_response->line, "+ERR", 4U))
                {
                    result |= PROTO_GM800_RESPONSE_ERROR;
                }
                if((1U == step && (0 == strcmp(p_response->line, "+ok") ||
                                   0 == strcmp(p_response->line, "a+ok"))) ||
                   (step >= 2U && 0 == strcmp(p_response->line, "OK")))
                {
                    result |= PROTO_GM800_RESPONSE_OK;
                }
            }
            p_response->length   = 0U;
            p_response->overflow = 0U;
        }
        else if(p_response->length < sizeof(p_response->line) - 1U)
        {
            p_response->line[p_response->length++] = (char)value;
        }
        else
        {
            p_response->overflow = 1U;
        }
    }
    return result;
}

proto_gm800_request_t proto_gm800_parse_request(const uint8_t *p_data,
                                                uint16_t       length,
                                                char          *p_identity,
                                                uint16_t       capacity)
{
    static const char g_prefix[] = "AT+CLOUD=";
    uint16_t          index;
    if(NULL == p_data || length < 8U || 0 != memcmp(p_data, g_prefix, 8U))
    {
        return PROTO_GM800_NOT_REQUEST;
    }
    if(NULL == p_identity || capacity < PROTO_GM800_ID_SIZE + 1U ||
       length < 9U + PROTO_GM800_ID_SIZE || 0 != memcmp(p_data, g_prefix, 9U))
    {
        return PROTO_GM800_INVALID_REQUEST;
    }
    for(index = 0U; index < PROTO_GM800_ID_SIZE; index++)
    {
        if(p_data[9U + index] < '0' || p_data[9U + index] > '9')
        {
            return PROTO_GM800_INVALID_REQUEST;
        }
    }
    memcpy(p_identity, p_data + 9U, PROTO_GM800_ID_SIZE);
    p_identity[PROTO_GM800_ID_SIZE] = '\0';
    return PROTO_GM800_VALID_REQUEST;
}
uint8_t proto_gm800_command(uint8_t     step,
                            const char *p_identity,
                            uint8_t    *p_output,
                            uint16_t    capacity,
                            uint16_t   *p_length)
{
    const char *p_command;
    size_t      length;
    if(NULL == p_length)
    {
        return 0U;
    }
    *p_length = 0U;
    if(NULL == p_output || step >= sizeof(p_commands) / sizeof(p_commands[0U]))
    {
        return 0U;
    }
    p_command = p_commands[step];
    if(NULL != p_command)
    {
        length = strlen(p_command);
        if(length > capacity)
        {
            return 0U;
        }
        memcpy(p_output, p_command, length);
    }
    else
    {
        static const char g_prefix[] = "AT+CLOUD=";
        static const char g_suffix[] = GM800_CLOUD_AUTH_SUFFIX;
        uint16_t          index;
        length = 9U + PROTO_GM800_ID_SIZE + sizeof(g_suffix) - 1U;
        if(NULL == p_identity || length > capacity)
        {
            return 0U;
        }
        for(index = 0U; index < PROTO_GM800_ID_SIZE; index++)
        {
            if(p_identity[index] < '0' || p_identity[index] > '9')
            {
                return 0U;
            }
        }
        memcpy(p_output, g_prefix, 9U);
        memcpy(p_output + 9U, p_identity, PROTO_GM800_ID_SIZE);
        memcpy(p_output + 9U + PROTO_GM800_ID_SIZE, g_suffix,
               sizeof(g_suffix) - 1U);
    }
    *p_length = (uint16_t)length;
    return 1U;
}

/* end of file --------------------------------------------------------------*/
