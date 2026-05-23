#ifndef _WS_SERVER_H
#define _WS_SERVER_H

#include "esp_err.h"

typedef void(*ws_recive_cb)(uint8_t* payload, int len);

typedef struct 
{
    const char* html_code;
    ws_recive_cb receive_fn;
}ws_cfg_t;


esp_err_t web_ws_start(ws_cfg_t* cfg);

esp_err_t web_ws_stop(void);

esp_err_t web_ws_send(uint8_t *data, int len);

#endif