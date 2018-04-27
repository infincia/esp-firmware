#include "globals.hpp"

/* FreeRTOS event group to signal when we are connected & ready to make a request */
EventGroupHandle_t wifi_event_group;
EventBits_t CONNECTED_BIT = BIT0;

/* FreeRTOS event group to signal when the device is provisioned */
EventGroupHandle_t provisioning_event_group;
EventBits_t PROVISIONED_BIT = BIT0;


QueueHandle_t volumeChangeQueue;
QueueHandle_t displayQueue;

#if defined(USE_WEB)
QueueHandle_t webQueue;
#endif

#if defined(USE_AWS)
QueueHandle_t awsQueue;
#endif

#if defined(USE_HOMEKIT)
QueueHandle_t homekitQueue;
#endif

QueueHandle_t ledQueue;

