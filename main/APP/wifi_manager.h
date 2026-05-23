#ifndef __WIFI_MANAGER_H
#define __WIFI_MANAGER_H

#include "esp_wifi.h"
typedef void (*p_wifi_scan_cb)(int num, wifi_ap_record_t *ap_record_t);

void wifi_manager_init(void);
void wifi_manager_connect(const char* ssid, const char* password);
esp_err_t wifi_manager_ap(void);
esp_err_t wifi_manager_scan(p_wifi_scan_cb f);

#endif