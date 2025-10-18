#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "driver/gpio.h"
#include <string.h>
#include "wifi_event_group.h"
#include <ctype.h>

#define PORT 3000
#define BLINK_LED 32
static const char *TAG = "tcp_server";

// Funkcja oczyszczająca buffer z NULL i białych znaków na końcu
void strip_null_and_whitespace(char *str) {
    int len = strlen(str);
    for (int i = 0; i < len; i++) {
        if (str[i] == '\0' || str[i] == 0) {
            str[i] = '\0';
            len = i;
            break;
        }
    }
    // Usuwanie białych znaków na końcu
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[len - 1] = '\0';
        len--;
    }
}


void tcp_server_task(void *pvParameter)
{
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if ((bits & WIFI_CONNECTED_BIT) == 0)
    {
        ESP_LOGE(TAG, "WiFi not connected, aborting tcp_server_task");
        vTaskDelete(NULL);
        return;
    }

    int listen_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char rx_buffer[128];
    int len;

    listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock < 0)
    {
        ESP_LOGE(TAG, "Failed to create socket");
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        ESP_LOGE(TAG, "Socket bind failed");
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    if (listen(listen_sock, 1) < 0)
    {
        ESP_LOGE(TAG, "Socket listen failed");
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "TCP server listening on port %d", PORT);

    while (1)
    {
        client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &addr_len);
        if (client_sock < 0)
        {
            ESP_LOGE(TAG, "Socket accept failed");
            continue;
        }

        ESP_LOGI(TAG, "Client connected");

        while (1)
        {
            len = recv(client_sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            if (len <= 0)
            {
                ESP_LOGI(TAG, "Client disconnected");
                break;
            }

            rx_buffer[len] = '\0'; // null terminator
            strip_null_and_whitespace(rx_buffer);

            ESP_LOGI(TAG, "Received: %s", rx_buffer);
            // ESP_LOGI(TAG, "Received (hex):");
            // for (int i = 0; i < len; i++)
            // {
            //     ESP_LOGI(TAG, "%02X ", (unsigned char)rx_buffer[i]);
            // }

            if (strcasecmp(rx_buffer, "LED_ON") == 0)
            {
                gpio_set_level(BLINK_LED, 0);
                ESP_LOGI(TAG, "LED ON");
            }
            else if (strcasecmp(rx_buffer, "LED_OFF") == 0)
            {
                gpio_set_level(BLINK_LED, 1);
                ESP_LOGI(TAG, "LED OFF");
            }
            else
            {
                ESP_LOGI(TAG, "Unknown command");
            }
        }

        close(client_sock);
    }
}
