#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <string.h>
#include <unistd.h>
#include "driver/gpio.h"

#define BLINK_LED 32
static const char *TAG = "tcp_cmd";

extern EventGroupHandle_t wifi_event_group;

void tcp_command_task(void *pvParameter)
{
    const char* server_hostname = "WachcioDrop";
    const uint16_t server_port = 3000;

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           BIT0,    // WIFI_CONNECTED_BIT
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if ((bits & BIT0) == 0) {
        ESP_LOGE(TAG, "WiFi not connected, aborting tcp_command_task");
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        struct sockaddr_in server_addr;
        int sock;
        char buf[128];
        int len;

        struct addrinfo hints = {
            .ai_family = AF_INET,
            .ai_socktype = SOCK_STREAM,
        };

        struct addrinfo *res = NULL;
        int err = getaddrinfo(server_hostname, NULL, &hints, &res);
        if (err != 0 || res == NULL) {
            ESP_LOGE(TAG, "DNS resolution failed for %s, err=%d", server_hostname, err);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        struct sockaddr_in *addr = (struct sockaddr_in *)res->ai_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port);
        server_addr.sin_addr.s_addr = addr->sin_addr.s_addr;

        freeaddrinfo(res);

        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock < 0) {
            ESP_LOGE(TAG, "Socket creation failed");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        ESP_LOGI(TAG, "Connecting to server %s:%d", server_hostname, server_port);

        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            ESP_LOGE(TAG, "TCP connect failed");
            close(sock);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        ESP_LOGI(TAG, "Connected to %s", server_hostname);

        while (1) {
            len = read(sock, buf, sizeof(buf) - 1);
            if (len <= 0) {
                ESP_LOGE(TAG, "Disconnected from server");
                close(sock);
                break;
            }

            buf[len] = '\0';
            ESP_LOGI(TAG, "Command received: %s", buf);

            if (strcmp(buf, "LED_ON") == 0) {
                gpio_set_level(BLINK_LED, 1);
                ESP_LOGI(TAG, "LED ON");
            } else if (strcmp(buf, "LED_OFF") == 0) {
                gpio_set_level(BLINK_LED, 0);
                ESP_LOGI(TAG, "LED OFF");
            } else {
                ESP_LOGI(TAG, "Unknown command");
            }
        }

        ESP_LOGI(TAG, "Reconnecting in 5 seconds...");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
