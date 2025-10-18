#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "http_server_task.h"

#include "driver/gpio.h"

extern void wifi_task(void *pvParameter);
extern void tcp_server_task(void *pvParameter);

#define BLINK_LED 32

void init_gpio(void)
{
    gpio_reset_pin(BLINK_LED);
    gpio_set_direction(BLINK_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(BLINK_LED, 1); // LED off (active low)
}

void app_main(void)
{
    init_gpio(); // Inicjalizacja pinu LED jako output, poziom wysoki (OFF)
    // gpio_reset_pin(BLINK_LED);
    // gpio_set_direction(BLINK_LED, GPIO_MODE_OUTPUT);

    // xTaskCreate(wifi_task, "wifi_task", 4096, NULL, 5, NULL);
    // xTaskCreate(tcp_server_task, "tcp_server_task", 8192, NULL, 5, NULL);
    xTaskCreate(wifi_task, "wifi_task", 4096, NULL, 5, NULL);
    xTaskCreate(http_server_task, "http_server_task", 8192, NULL, 5, NULL);
}
