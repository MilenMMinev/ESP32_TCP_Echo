#ifndef WIFIAP_H
#define WIFIAP_H

#include <string.h>
#include "freertos/FreeRTOS.h"
// #include "netdb.h"
#include "errno.h"
// #include "esp_system.h"
// #include "esp_event.h"
#include "esp_log.h"
// #include "nvs_flash.h"
#include "esp_wifi.h"


#define WIFI_SSID "ESP32_TCP"
#define WIFI_PASS ""
#define WIFI_CHANNEL 13
#define MAX_WIFI_CONN 5

static const char *TAG = "WIFI_AP";


void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void wifi_init_softap(void);

#endif //WIFIAP_H