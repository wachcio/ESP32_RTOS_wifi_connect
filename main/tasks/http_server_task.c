#include "esp_http_server.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "cJSON.h"

#ifndef httpd_req_get_user_ctx
#define httpd_req_get_user_ctx(req) ((req)->user_ctx)
#endif

#define BLINK_LED 32
static const char *TAG = "http_server";

// Definicje mastek dla metod
#define ALLOW_GET  (1 << HTTP_GET)
#define ALLOW_POST (1 << HTTP_POST)
#define ALLOW_ALL  (ALLOW_GET | ALLOW_POST)

extern void init_gpio(void);

typedef esp_err_t (*json_command_handler_t)(cJSON *root, char *resp, size_t sz);

typedef struct {
    const char *name;
    json_command_handler_t handler;
    const char *description;
    const char *log_fmt;
    esp_log_level_t log_lvl;
    int allowed_methods; // maska dozwolonych metod HTTP, np. ALLOW_GET | ALLOW_POST
} json_command_t;

typedef struct {
    const char *uri;
    httpd_method_t method;
    esp_err_t (*handler)(httpd_req_t *req);
    const json_command_t *cmds;
    size_t num_cmds;
    const char *tag;
} http_endpoint_t;


//POST REQUEST

static esp_err_t cmd_led_on(cJSON *root, char *resp, size_t sz) {
    gpio_set_level(BLINK_LED, 1);
    snprintf(resp, sz, "{\"status\":\"LED_ON\"}");
    return ESP_OK;
}

static esp_err_t cmd_led_off(cJSON *root, char *resp, size_t sz) {
    gpio_set_level(BLINK_LED, 0);
    snprintf(resp, sz, "{\"status\":\"LED_OFF\"}");
    return ESP_OK;
}

static esp_err_t cmd_led_toggle(cJSON *root, char *resp, size_t sz) {
    int lvl = gpio_get_level(BLINK_LED);
    gpio_set_level(BLINK_LED, !lvl);
    snprintf(resp, sz, "{\"status\":\"LED_TOGGLE\",\"old\":%d,\"new\":%d}", lvl, !lvl);
    return ESP_OK;
}

static esp_err_t cmd_led_blink(cJSON *root, char *resp, size_t sz) {
    int blink_count = 5;
    for (int i = 0; i < blink_count; i++) {
        gpio_set_level(BLINK_LED, 1);
        vTaskDelay(pdMS_TO_TICKS(200));
        gpio_set_level(BLINK_LED, 0);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    snprintf(resp, sz, "{\"status\":\"LED_BLINK\",\"blink_count\":%d}", blink_count);
    return ESP_OK;
}

// GET REQUEST
static esp_err_t led_status_get_handler(httpd_req_t *req)
{
    char resp[128];
    int lvl = gpio_get_level(BLINK_LED);
    snprintf(resp, sizeof(resp), "{\"status\":\"%s\",\"level\":%d}", lvl == 1 ? "ON" : "OFF", lvl);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, resp);
    return ESP_OK;
}


//TABLE OF COMMAND FOR POST REQUEST 
static const json_command_t led_cmds[] = {
    {"LED_ON",     cmd_led_on,     "Turn on LED",     "LED ON -> LOW",  ESP_LOG_INFO, ALLOW_POST},
    {"LED_OFF",    cmd_led_off,    "Turn off LED",    "LED OFF -> HIGH", ESP_LOG_INFO, ALLOW_POST},
    {"LED_TOGGLE", cmd_led_toggle, "Toggle LED",      "LED TOGGLE",     ESP_LOG_INFO, ALLOW_POST},
    {"LED_BLINK",  cmd_led_blink,  "LED BLINK",       "LED BLINK",      ESP_LOG_INFO, ALLOW_POST},
};

static esp_err_t generic_json_post_handler(httpd_req_t *req) {
    http_endpoint_t *ep = (http_endpoint_t *)httpd_req_get_user_ctx(req);

    char buf[512];
    char resp[256];

    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad JSON");
        return ESP_FAIL;
    }

    cJSON *c = cJSON_GetObjectItem(root, "command");
    if (!cJSON_IsString(c)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No command");
        return ESP_FAIL;
    }

    for (size_t i = 0; i < ep->num_cmds; ++i) {
        if (strcasecmp(c->valuestring, ep->cmds[i].name) == 0) {
            // Sprawdzamy czy metoda jest dozwolona
            if (!(ep->cmds[i].allowed_methods & (1 << req->method))) {
                httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Method Not Allowed");
                cJSON_Delete(root);
                return ESP_FAIL;
            }
            esp_log_write(ep->cmds[i].log_lvl, ep->tag, "%s", ep->cmds[i].log_fmt);
            esp_err_t res = ep->cmds[i].handler(root, resp, sizeof(resp));
            if (res == ESP_OK) {
                httpd_resp_set_type(req, "application/json");
                httpd_resp_sendstr(req, resp);
            } else {
                httpd_resp_send_500(req);
            }
            cJSON_Delete(root);
            return ESP_OK;
        }
    }

    cJSON_Delete(root);
    httpd_resp_sendstr(req, "{\"error\":\"Unknown command\"}");
    return ESP_OK;
}


//Endpoints
//     const char *uri — ścieżka URI endpointa (np. "/api_v1/led")
//     httpd_method_t method — metoda HTTP, taka jak HTTP_GET, HTTP_POST
//     esp_err_t (*handler)(httpd_req_t *req) — wskaźnik na funkcję obsługi danego endpointa, przyjmującą wskaźnik na żądanie HTTP
//     const json_command_t *cmds — wskaźnik do tablicy komend JSON obsługiwanych pod tym endpointem; NULL jeśli brak komend (np. w statycznym statusie GET)
//     size_t num_cmds — liczba komend w tablicy cmds; 0 jeśli brak komend
//     const char *tag — tag do logów identyfikujący endpoint

static http_endpoint_t endpoints[] = {
    {"/api_v1/led", HTTP_POST, generic_json_post_handler, led_cmds, sizeof(led_cmds)/sizeof(led_cmds[0]), "LED_POST"},
    {"/api_v1/led", HTTP_GET, led_status_get_handler, NULL, 0, "LED_STATUS_GET"},
};

void http_server_task(void *pvParameters) {
    init_gpio();

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return;
    }

    for (size_t i = 0; i < sizeof(endpoints) / sizeof(endpoints[0]); ++i) {
        httpd_uri_t uri_config = {
            .uri = endpoints[i].uri,
            .method = endpoints[i].method,
            .handler = endpoints[i].handler,
            .user_ctx = &endpoints[i]
        };
        httpd_register_uri_handler(server, &uri_config);
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
