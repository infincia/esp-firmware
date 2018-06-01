#pragma once

#define USE_OTA 1
#define USE_AWS 1
#define USE_WEB 1
#define USE_WEBSOCKET 1
#define USE_HOMEKIT 1

#define USE_AMP 1
#define USE_TEMPERATURE 1
// #define USE_CONSOLE 1

// #define USE_ESP_TLS 1

#define HARDCODE_WIFI 1

#define DEFAULT_VOLUME 27
#define MIN_VOLUME 0
#define MAX_VOLUME 63

// these are set for the onboard LED on the EZ-SBC boards
#define ERROR_GPIO 16
#define READY_GPIO 17
#define IDENTIFY_GPIO 18

#define LED_ON 1
#define LED_OFF 0

#define VOLUME_LEVEL_KEY "amp-volume"


#define WIFI_SSID_KEY "wifi-ssid"
#define WIFI_PASSWORD_KEY "wifi-password"

#define DEVICE_PROVISIONING_NAME_KEY "device-name"
#define DEVICE_PROVISIONING_TYPE_KEY "device-type"

#define MAX_HTTP_CONNECTIONS 5
#define DEFAULT_HTTP_PORT 80
