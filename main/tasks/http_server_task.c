#include "esp_http_server.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "cJSON.h" // Nagłówki biblioteki cJSON

#define BLINK_LED 32
static const char *TAG = "http_server";

// Konfiguracja GPIO (w app_main)
extern void init_gpio(void);

static esp_err_t led_post_handler(httpd_req_t *req)
{
    char buf[100];
    int ret, remaining = req->content_len;

    if (remaining >= sizeof(buf))
    {
        ESP_LOGE(TAG, "Payload too large");
        httpd_resp_send_408(req);
        return ESP_FAIL;
    }

    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0)
    {
        ESP_LOGE(TAG, "Failed to receive request body");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root)
    {
        ESP_LOGE(TAG, "Invalid JSON");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        return ESP_FAIL;
    }

    cJSON *cmd = cJSON_GetObjectItemCaseSensitive(root, "command");
    if (cJSON_IsString(cmd) && (cmd->valuestring != NULL))
    {
        if (strcasecmp(cmd->valuestring, "LED_ON") == 0)
        {
            gpio_set_level(BLINK_LED, 0); // active low
            ESP_LOGI(TAG, "LED ON");
            httpd_resp_sendstr(req, "{\"status\":\"LED_ON\"}");
        }
        else if (strcasecmp(cmd->valuestring, "LED_OFF") == 0)
        {
            gpio_set_level(BLINK_LED, 1);
            ESP_LOGI(TAG, "LED OFF");
            httpd_resp_sendstr(req, "{\"status\":\"LED_OFF\"}");
        }
        else if (strcasecmp(cmd->valuestring, "LED_TOGGLE") == 0)
        {
            int curr_level = gpio_get_level(BLINK_LED);
            ESP_LOGI(TAG, "Current LED level: %d", curr_level);
            gpio_set_level(BLINK_LED, !curr_level);
            ESP_LOGI(TAG, "New LED level: %d", !curr_level);
            ESP_LOGI(TAG, "LED TOGGLE");
            httpd_resp_sendstr(req, "{\"status\":\"LED_TOGGLE\"}");
        }
        else
        {
            ESP_LOGI(TAG, "Unknown command");
            httpd_resp_sendstr(req, "{\"error\":\"Unknown command\"}");
        }
    }
    else
    {
        ESP_LOGE(TAG, "JSON missing 'command'");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
    }

    cJSON_Delete(root);
    return ESP_OK;
}

static httpd_uri_t led_post_uri = {
    .uri = "/api_v1/led",
    .method = HTTP_POST,
    .handler = led_post_handler,
    .user_ctx = NULL};

static httpd_handle_t start_http_server(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    ESP_LOGI(TAG, "Starting HTTP Server");
    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_register_uri_handler(server, &led_post_uri);
        return server;
    }

    ESP_LOGE(TAG, "Failed to start HTTP Server");
    return NULL;
}

void http_server_task(void *pvParameters)
{
    init_gpio();
    start_http_server();

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
