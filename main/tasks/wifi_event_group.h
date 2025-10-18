#ifndef WIFI_EVENT_GROUP_H
#define WIFI_EVENT_GROUP_H

#include "freertos/event_groups.h"

extern EventGroupHandle_t wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#endif // WIFI_EVENT_GROUP_H
