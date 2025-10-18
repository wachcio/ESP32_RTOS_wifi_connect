#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

extern void wifi_task(void *pvParameter);
extern void tcp_server_task(void *pvParameter);

#define BLINK_LED 32

void app_main(void)
{
    gpio_reset_pin(BLINK_LED);
    gpio_set_direction(BLINK_LED, GPIO_MODE_OUTPUT);

    xTaskCreate(wifi_task, "wifi_task", 4096, NULL, 5, NULL);
    xTaskCreate(tcp_server_task, "tcp_server_task", 8192, NULL, 5, NULL);
}
