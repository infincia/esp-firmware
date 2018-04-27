#pragma once

#include "pch.hpp"

/* FreeRTOS event group to signal when we are connected & ready to make a request */
extern EventGroupHandle_t wifi_event_group;
/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
extern EventBits_t CONNECTED_BIT;


/* FreeRTOS event group to signal when the device is provisioned */
extern EventGroupHandle_t provisioning_event_group;
extern EventBits_t PROVISIONED_BIT;


extern QueueHandle_t volumeChangeQueue;
extern QueueHandle_t displayQueue;


#if defined(USE_WEB)
extern QueueHandle_t webQueue;
#endif

#if defined(USE_AWS)
extern QueueHandle_t awsQueue;
#endif

#if defined(USE_HOMEKIT)
extern QueueHandle_t homekitQueue;
#endif

extern QueueHandle_t ledQueue;


