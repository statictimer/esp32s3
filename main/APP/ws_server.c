#include "ws_server.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include <string.h>

#define TAG     "ws_server"

//网页
static const char* http_html = NULL;

//websocket 接受数据回调函数
static ws_recive_cb ws_receive_fn = NULL;

static int client_fds = -1;

static httpd_handle_t server_handle;

esp_err_t get_http_req(httpd_req_t *r)
{
    return httpd_resp_send(r, http_html, HTTPD_RESP_USE_STRLEN);
}

esp_err_t handler_ws_req(httpd_req_t *r)
{
    if(r ->method == HTTP_GET)
    {
        client_fds = httpd_req_to_sockfd(r);
        return ESP_OK;
    }

    httpd_ws_frame_t pkt;
    esp_err_t ret;
    memset(&pkt, 0, sizeof(pkt));
    ret = httpd_ws_recv_frame(r, &pkt, 0);
    if(ret != ESP_OK)
    {
        return ret;
    }
    uint8_t *buf = (uint8_t*)malloc(pkt.len + 1);
    if(buf == NULL)
    {
        ESP_LOGI(TAG, "Memory allocation failed");
        return ESP_FAIL;
    }
    pkt.payload = buf;
    ret = httpd_ws_recv_frame(r, &pkt, pkt.len);
    if(ret == ESP_OK)
    {
        if(pkt.type == HTTPD_WS_TYPE_TEXT)
        {
            ESP_LOGI(TAG, "Get websocket message:%s",pkt.payload);
            if(ws_receive_fn)
                ws_receive_fn(pkt.payload, pkt.len);
        }
    }
    free(buf);
    return ESP_OK;
}

esp_err_t web_ws_start(ws_cfg_t* cfg)
{
    if(cfg == NULL || cfg->html_code == NULL)
        return ESP_FAIL;

    // 如果服务器已经启动，先停止
    if (server_handle != NULL) {
        ESP_LOGW(TAG, "HTTP server already running, stopping it first");
        web_ws_stop();
    }

    http_html = cfg->html_code;
    ws_receive_fn = cfg->receive_fn;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    // 可指定端口，避免冲突
    config.server_port = 80;
    config.lru_purge_enable = true;

    esp_err_t ret = httpd_start(&server_handle, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    httpd_uri_t uri_get = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = get_http_req
    };
    ret = httpd_register_uri_handler(server_handle, &uri_get);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register / handler");
        httpd_stop(server_handle);
        server_handle = NULL;
        return ret;
    }

    httpd_uri_t uri_ws = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = handler_ws_req,
        .is_websocket = true
    };
    ret = httpd_register_uri_handler(server_handle, &uri_ws);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /ws handler");
        httpd_stop(server_handle);
        server_handle = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "HTTP server started successfully");
    return ESP_OK;
}

esp_err_t web_ws_stop(void)
{
    if(server_handle)
    {
        httpd_stop(server_handle);
        server_handle = NULL;
    }
    return ESP_OK;
}

esp_err_t web_ws_send(uint8_t *data, int len)
{
    httpd_ws_frame_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.payload = data;
    pkt.len = len;
    pkt.type = HTTPD_WS_TYPE_TEXT;
    return httpd_ws_send_data(server_handle, client_fds, &pkt);
}
