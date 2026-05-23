#include"ap_wifi.h"
#include "esp_spiffs.h"
#include <sys/stat.h>
#include <string.h>
#include "ws_server.h"
#include <cJSON.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "ap_wifi"

#define SPIFF_MOUNT     "/spiffs"
#define HTML_PATH       "/spiffs/apcfg.html"

static char current_ssid[32];
static char current_password[64];

static char* html_code = NULL;
static EventGroupHandle_t apcfg_event;
#define APCFG_BIT   (BIT0)

static char *init_web_papg_buffer(void)
{
    esp_vfs_spiffs_conf_t conf =
    {
        .base_path = SPIFF_MOUNT,
        .format_if_mount_failed = false,
        .max_files = 3,
        .partition_label = NULL,
    };
    esp_vfs_spiffs_register(&conf);
    struct stat st;
    if(stat(HTML_PATH, &st))
    {
        return NULL;
    }
    char* buf = (char*)malloc(st.st_size + 1);
    memset(buf, 0, st.st_size + 1);
    FILE *fp = fopen(HTML_PATH, "r");
    if(fp)
    {
        if(fread(buf, st.st_size, 1, fp) == 0)
        {
            free(buf);
            
        }
        fclose(fp);
    }
    else
    {
        free(buf);
        buf = NULL;
    }
    return buf;
}

static void ap_wifi_task(void* param)
{
    EventBits_t ev;
    while(1)
    {   
        ev = xEventGroupWaitBits(apcfg_event, APCFG_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(10*1000));
        if(ev & APCFG_BIT)
        {
            web_ws_stop();
            wifi_manager_connect(current_ssid, current_password);
        }
    }
}

void ap_wifi_init(void)
{
    wifi_manager_init();
    html_code = init_web_papg_buffer();
    if (html_code == NULL) {
        ESP_LOGE(TAG, "Failed to load html file from /spiffs/apcfg.html");
    } else {
        ESP_LOGI(TAG, "HTML loaded");
    }
    apcfg_event = xEventGroupCreate();
    xTaskCreatePinnedToCore(ap_wifi_task, "ap_wifi_task", 4096, NULL, 3, NULL, 1);

}

void wifi_receive_handle(int num, wifi_ap_record_t *ap_record)
{
    cJSON* root = cJSON_CreateObject();
    cJSON* wifilist_js = cJSON_AddArrayToObject(root, "wifi_list");
    for(int i = 0; i < num; i++)
    {
        cJSON* wifi_js = cJSON_CreateObject();
        cJSON_AddStringToObject(wifi_js, "ssid", (char*)ap_record[i].ssid);
        cJSON_AddNumberToObject(wifi_js, "rssi", ap_record[i].rssi);//信号强度
       
        cJSON_AddBoolToObject(wifi_js, "encrypted", (ap_record[i].authmode == WIFI_AUTH_OPEN) ? 0 : 1);
        cJSON_AddItemToArray(wifilist_js, wifi_js);
    }
    char * data = cJSON_Print(root);
    ESP_LOGI(TAG, "WS send:%s", data);
    web_ws_send((uint8_t*)data, strlen(data));
    cJSON_free(data);
    cJSON_Delete(root);
}

static void ws_receive_handle(uint8_t* payload, int len)
{
    cJSON* root = cJSON_Parse((char*)payload);
    if(root)
    {
        cJSON* scan_js = cJSON_GetObjectItem(root, "scan");
        cJSON* ssid_is = cJSON_GetObjectItem(root, "ssid");
        cJSON* password_js = cJSON_GetObjectItem(root, "password");
        if(scan_js)
        {
            char* scan_value = cJSON_GetStringValue(scan_js);
            {
                if(strcmp(scan_value, "start") == 0)
                {
                    //启动扫描
                    wifi_manager_scan(wifi_receive_handle);
                }
            }
        }
        if(ssid_is && password_js)
        {
            char* ssid_value = cJSON_GetStringValue(ssid_is);
            char* password_value = cJSON_GetStringValue(password_js);
            snprintf(current_ssid, sizeof(current_ssid), "%s", ssid_value);
            snprintf(current_password, sizeof(current_password), "%s",password_value);
            xEventGroupSetBits(apcfg_event, APCFG_BIT);
        }
    }
}

//进入配网模式
void ap_wifi_apcfg(void)
{
    wifi_manager_ap();
    ws_cfg_t ws_cfg =
    {
        .html_code = html_code,
        .receive_fn = ws_receive_handle,
    };
    web_ws_start(&ws_cfg);

}