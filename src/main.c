#include "nvs_flash.h"
#include "wifi_ap.h"
#include "server.h"


void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init_softap();

    SemaphoreHandle_t server_ready = xSemaphoreCreateBinary();
    assert(server_ready);
    xTaskCreate(tcp_server_task, "tcp_server", 4096, &server_ready, 5, NULL);
    xSemaphoreTake(server_ready, portMAX_DELAY);
    vSemaphoreDelete(server_ready);
}
