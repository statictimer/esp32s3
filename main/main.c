#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "led.h"
#include "lcd.h"
#include <netdb.h>
#include "ap_wifi.h"
#include "esp_task_wdt.h"

i2c_obj_t i2c0_master;

uint8_t key_num;

static void key_task(void* param)
{
    while(1)
    {
        ESP_LOGI("MAIN", "Before key_scan");
        key_num = xl9555_key_scan(0);
        ESP_LOGI("MAIN", "After key_scan, key=%d", key_num);
        if(key_num == KEY0_PRES)
        {
            ESP_LOGI("MAIN", "Key pressed, enter apcfg");
            ap_wifi_apcfg();
        }
        LED_TOGGLE();
        vTaskDelay(500);
    }
}

/**
 * @brief       程序入口
 * @param       无
 * @retval      无
 */
void app_main(void)
{
    esp_err_t ret;

    ret = nvs_flash_init();             /* 初始化NVS */

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    led_init();                         /* 初始化LED */
    i2c0_master = iic_init(I2C_NUM_0);  /* 初始化IIC0 */
    spi2_init();                        /* 初始化SPI2 */
    xl9555_init(i2c0_master);           /* IO扩展芯片初始化 */
    lcd_init();                         /* 初始化LCD */

    lcd_show_string(0, 0, 240, 32, 32, "ESP32-S3", RED);
    
    ap_wifi_init();
    
    xTaskCreatePinnedToCore(key_task, "key_task", 4096, NULL, 4, NULL, 1);
    
    esp_task_wdt_deinit();
    

    while (1)
    {
        
    }
}