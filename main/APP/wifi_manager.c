#include "wifi_manager.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "lcd.h"
#include "lwip/ip4_addr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

//
static SemaphoreHandle_t scan_semap;

//ESP32作为热点
/* wifi名称 */
#define DEFAULT_SSID        "ESP32_S3"
/* wifi密码 */
#define DEFAULT_PWD         "ESP32_12345678"
/* 事件标志 */
static EventGroupHandle_t   wifi_event;
#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
static const char *TAG = "wifi_manager";
char lcd_buff[100] = {0};

static esp_netif_t *ap_netif = NULL;

/**
 * @brief       链接显示
 * @param       flag:2->链接;1->链接失败;0->再链接中
 * @retval      无
 */
static void connet_display(uint8_t flag)
{
    if(flag == 2)
    {
        lcd_fill(0,90,320,240,WHITE);
        sprintf(lcd_buff, "ssid:%s",DEFAULT_SSID);
        lcd_show_string(0, 90, 240, 16, 16, lcd_buff, BLUE);
        sprintf(lcd_buff, "psw:%s",DEFAULT_PWD);
        lcd_show_string(0, 110, 240, 16, 16, lcd_buff, BLUE);
    }
    else if (flag == 1)
    {
        lcd_show_string(0, 90, 240, 16, 16, "wifi connecting fail", BLUE);
    }
    else
    {
        lcd_show_string(0, 90, 240, 16, 16, "wifi connecting......", BLUE);
    }
}

/**
 * @brief       WIFI链接糊掉函数
 * @param       arg:传入网卡控制块
 * @param       event_base:WIFI事件
 * @param       event_id:事件ID
 * @param       event_data:事件数据
 * @retval      无
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    static int s_retry_num = 0;

    /* 扫描到要连接的WIFI事件 */
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        connet_display(0);
        esp_wifi_connect();
    }
    /* 连接WIFI事件 */
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        connet_display(2);
    }
    /* 连接WIFI失败事件 */
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        /* 尝试连接 */
        if (s_retry_num < 20)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(wifi_event, WIFI_FAIL_BIT);
        }

        ESP_LOGI(TAG,"connect to the AP fail");
    }
    else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED)
    {
         ESP_LOGI(TAG, "sta device connected");
    }
    else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
         ESP_LOGI(TAG, "sta device disconnected");
    }
    /* 工作站从连接的AP获得IP */
    else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "static ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event, WIFI_CONNECTED_BIT);
    }
}

/**
 * @brief       WIFI初始化
 * @param       无
 * @retval      无
 */
void wifi_manager_init(void)
{
    static esp_netif_t *sta_netif = NULL;
    
    wifi_event= xEventGroupCreate();    /* 创建一个事件标志组 */
    /* TCP/IP协议栈的初始化 */
    ESP_ERROR_CHECK(esp_netif_init());
    /* 创建新的事件循环 */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    /* 创建STA对象 */
    sta_netif= esp_netif_create_default_wifi_sta();
    assert(sta_netif);
    
    /* 创建AP对象 */
    ap_netif= esp_netif_create_default_wifi_ap();
    assert(ap_netif);
    if (ap_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create AP netif");
    } else {
        ESP_LOGI(TAG, "AP netif created successfully");
    }


    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    /* 注册 WiFi 所有事件的监听 */
    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL) );
    /* 注册 获取IP 这个特定事件的监听 */
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL) );
    /* 初始化WIFI */
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));    
   
    scan_semap = xSemaphoreCreateBinary();
    xSemaphoreGive(scan_semap);   // 添加这一行，使信号量初始可用
}

void wifi_manager_connect(const char* ssid, const char* password)
{
    wifi_config_t  wifi_config = 
    {
        .sta = 
        {                                                          
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,   
        },   
    };
    snprintf((char*)wifi_config.sta.ssid, 32, "%s", ssid);
    snprintf((char*)wifi_config.sta.password, 64, "%s", password);
    /* 设置WIFI的工作模式为STA */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    /* 启动WIFI工作 */
    ESP_ERROR_CHECK(esp_wifi_start());

    /* 等待链接成功后、ip生成 */
    EventBits_t bits = xEventGroupWaitBits(wifi_event,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* 判断连接事件 */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 DEFAULT_SSID, DEFAULT_PWD);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        connet_display(1);
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 DEFAULT_SSID, DEFAULT_PWD);
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    vEventGroupDelete(wifi_event);
    xSemaphoreGive(scan_semap);
}

esp_err_t wifi_manager_ap(void)
{
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    if(mode == WIFI_MODE_APSTA)
    {
        ESP_LOGI(TAG,"wifi mode is ap + sta");
        // return ESP_OK;
    }
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_APSTA);

    wifi_config_t wifi_config =
    {
        .ap =
        {
            .channel = 6,
            .max_connection = 3,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        }
    };
    snprintf((char*)wifi_config.ap.ssid, 32, "%s", DEFAULT_SSID);
    wifi_config.ap.ssid_len = strlen(DEFAULT_SSID);
    snprintf((char*)wifi_config.ap.password, 64, "%s", DEFAULT_PWD);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);

    esp_netif_ip_info_t ipInfo;
    //设置IP地址
    IP4_ADDR(&ipInfo.ip, 192, 168, 100, 1);
    //设置网关地址
    IP4_ADDR(&ipInfo.gw, 192, 168, 100, 1);
    //设置子网掩码
    IP4_ADDR(&ipInfo.netmask, 255, 255, 255, 0);

    esp_netif_dhcps_stop(ap_netif);
    esp_netif_set_ip_info(ap_netif, &ipInfo);
    esp_netif_dhcps_start(ap_netif);

    esp_wifi_start();

    return ESP_OK;
}

void wifi_scan(void *param)
{
    p_wifi_scan_cb callback = (p_wifi_scan_cb)param;
    uint16_t ap_count = 0;
    uint16_t ap_num = 20;
    wifi_ap_record_t *ap_list = (wifi_ap_record_t*)malloc(sizeof(wifi_ap_record_t) * ap_num);
    esp_wifi_scan_start(NULL, true);
    esp_wifi_scan_get_ap_num(&ap_count);
    esp_wifi_scan_get_ap_records(&ap_num, ap_list);
    ESP_LOGI(TAG, "Total ap count :%d, actual ap number:%d", ap_count, ap_num);
    if(callback)
        callback(ap_count, ap_list);
    free(ap_list);
    xSemaphoreGive(scan_semap);
    vTaskDelete(NULL);
}

esp_err_t wifi_manager_scan(p_wifi_scan_cb f)
{
    if(xSemaphoreTake(scan_semap, 0))
    {
        esp_wifi_clear_ap_list();
        return xTaskCreatePinnedToCore(wifi_scan, "wifi_scan", 8192, f, 2, NULL, 1);
    }
    return ESP_OK;
}