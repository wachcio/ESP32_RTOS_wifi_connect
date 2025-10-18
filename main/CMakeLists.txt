file(READ "${CMAKE_CURRENT_LIST_DIR}/../.env" ENV_CONTENTS)

string(REGEX MATCH "SSID=([^\n\r]+)" _ ${ENV_CONTENTS})
set(WIFI_SSID "${CMAKE_MATCH_1}")

string(REGEX MATCH "PASSWORD=([^\n\r]+)" _ ${ENV_CONTENTS})
set(WIFI_PASSWORD "${CMAKE_MATCH_1}")

idf_component_register(SRCS "main.c" "tasks/wifi_task.c" "tasks/http_server_task.c"
                      INCLUDE_DIRS "." "tasks"
                      REQUIRES nvs_flash esp_wifi esp_http_server cjson driver)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
                   WIFI_SSID="${WIFI_SSID}"
                   WIFI_PASSWORD="${WIFI_PASSWORD}")