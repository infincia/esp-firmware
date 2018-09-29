#include "pch.hpp"

#include "led.hpp"

#if defined(CONFIG_FIRMWARE_USE_CONSOLE)
#include "console.hpp"
#endif

#if defined(CONFIG_FIRMWARE_USE_WEB)
#include "web.hpp"
#endif

#if defined(CONFIG_FIRMWARE_USE_AMP)
#include "amp.hpp"
#include "ir.hpp"
#include "oled.hpp"
#endif

#if defined(CONFIG_FIRMWARE_USE_TEMPERATURE_SI7021) || defined(CONFIG_FIRMWARE_USE_TEMPERATURE_DHT11)
#include "temperature.hpp"
#endif

#if defined(CONFIG_FIRMWARE_USE_OTA)
#include "updater.hpp"
#endif

#if defined(CONFIG_FIRMWARE_USE_AWS)
#include "aws/aws.hpp"
#endif

#if defined(CONFIG_FIRMWARE_USE_HOMEKIT)
#include "homekit/homekit.hpp"
#endif


static const char *TAG = "[Main]";

TaskHandle_t stats_task_handle;

TaskHandle_t setup_task_handle;

/**
 * Tasks
 */

LED led;

#if defined(CONFIG_FIRMWARE_USE_UDP_LOGGING)
#include "udp_logging.h"
#endif

#if defined(CONFIG_FIRMWARE_USE_CONSOLE)
Console console;
#endif

#if defined(CONFIG_FIRMWARE_USE_AMP)
Amp amp;
IR ir;
OLED oled;
#endif

#if defined(CONFIG_FIRMWARE_USE_TEMPERATURE_SI7021) || defined(CONFIG_FIRMWARE_USE_TEMPERATURE_DHT11)
#if !defined(CONFIG_FIRMWARE_TEMPERATURE_ENDPOINT_URL)
#error "Missing temperature endpoint URL"
#else
static const char* TEMPERATURE_ENDPOINT_URL = CONFIG_FIRMWARE_TEMPERATURE_ENDPOINT_URL;
#endif

Temperature temperature(TEMPERATURE_ENDPOINT_URL);
#endif

#if defined(CONFIG_FIRMWARE_USE_WEB)
Web web(CONFIG_FIRMWARE_WEB_PORT);
#endif

#if defined(CONFIG_FIRMWARE_USE_OTA)

#if !defined(CONFIG_FIRMWARE_OTA_MANIFEST_URL)
#error "Missing OTA manifest URL"
#else
static const char* UPDATE_MANIFEST_URL = CONFIG_FIRMWARE_OTA_MANIFEST_URL;
#endif

Updater updater(UPDATE_MANIFEST_URL);

#endif

#if defined(CONFIG_FIRMWARE_USE_AWS)
AWS aws;
#endif

#if defined(CONFIG_FIRMWARE_USE_HOMEKIT)
Homekit homekit;
#endif



/**
 * Wifi events
 */

static esp_err_t event_handler(void *ctx, system_event_t *event) {
    static const char *_TAG = "[WiFi]";

    switch (event->event_id) {
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(_TAG, "got IP");
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            #if defined(CONFIG_FIRMWARE_USE_UDP_LOGGING)
            udp_logging_init(CONFIG_FIRMWARE_UDP_LOGGING_IP, CONFIG_FIRMWARE_UDP_LOGGING_PORT);
            #endif
            break;
        case SYSTEM_EVENT_STA_LOST_IP:
            ESP_LOGI(_TAG, "lost IP");
            // xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_STA_START: {
            ESP_LOGI(_TAG, "STA start");
            esp_err_t err = esp_wifi_connect();
            switch (err) {
                case ESP_OK:
                break;
                case ESP_ERR_WIFI_NOT_INIT:
                    ESP_LOGI(_TAG, "ESP_ERR_WIFI_NOT_INIT");
                    break;
                case ESP_ERR_WIFI_NOT_STARTED:
                    ESP_LOGI(_TAG, "ESP_ERR_WIFI_NOT_STARTED");
                    break;
                case ESP_ERR_WIFI_CONN:
                    ESP_LOGI(_TAG, "ESP_ERR_WIFI_CONN");
                    break;
                case ESP_ERR_WIFI_SSID:
                    ESP_LOGI(_TAG, "ESP_ERR_WIFI_SSID");
                    break;
                default:
                break;
            }
            break;
        }
        case SYSTEM_EVENT_STA_CONNECTED:
            ESP_LOGI(_TAG, "STA connected");
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED: {
            ESP_LOGI(_TAG, "STA disconnected");
            udp_logging_free();
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            // This is a workaround as ESP32 WiFi libs don't currently auto-reassociate.
            esp_err_t err = esp_wifi_connect();
            switch (err) {
                case ESP_OK:
                break;
                case ESP_ERR_WIFI_NOT_INIT:
                    ESP_LOGI(_TAG, "ESP_ERR_WIFI_NOT_INIT");
                    break;
                case ESP_ERR_WIFI_NOT_STARTED:
                    ESP_LOGI(_TAG, "ESP_ERR_WIFI_NOT_STARTED");
                    break;
                case ESP_ERR_WIFI_CONN:
                    ESP_LOGI(_TAG, "ESP_ERR_WIFI_CONN");
                    break;
                case ESP_ERR_WIFI_SSID:
                    ESP_LOGI(_TAG, "ESP_ERR_WIFI_SSID");
                    break;
                default:
                    break;
            }
            break;
        }
        default:
            ESP_LOGI(_TAG, "event occurred, unknown type %d", event->event_id);
            break;
    }
    return ESP_OK;
}


/**
 * High level initialization
 *
 */



void setup_queues() {
    #if defined(CONFIG_FIRMWARE_USE_AMP)
    volumeChangeQueue = xQueueCreate(1, sizeof(IPCMessage));
    if (volumeChangeQueue == nullptr) {
        ESP_LOGE(TAG, "failed to create volume queue, restarting");
        abort();
    }

    displayQueue = xQueueCreate(1, sizeof(IPCMessage));
    if (displayQueue == nullptr) {
        ESP_LOGE(TAG, "failed to create display queue, restarting");
        abort();
    }
    #endif

    #if defined(CONFIG_FIRMWARE_USE_WEB)
    webQueue = xQueueCreate(1, sizeof(IPCMessage));
    if (webQueue == nullptr) {
        ESP_LOGE(TAG, "failed to create web queue, restarting");
        abort();
    }
    #endif

    #if defined(CONFIG_FIRMWARE_USE_AWS)
    awsQueue = xQueueCreate(1, sizeof(IPCMessage));
    if (awsQueue == nullptr) {
        ESP_LOGE(TAG, "failed to create aws queue, restarting");
        abort();
    }
    #endif

    #if defined(CONFIG_FIRMWARE_USE_HOMEKIT)
    homekitQueue = xQueueCreate(1, sizeof(IPCMessage));
    if (homekitQueue == nullptr) {
        ESP_LOGE(TAG, "failed to create homekit queue, restarting");
        abort();
    }
    #endif

    ledQueue = xQueueCreate(1, sizeof(IPCMessage));
    if (ledQueue == nullptr) {
        ESP_LOGE(TAG, "failed to create LED queue, restarting");
        abort();
    }
}


void setup_test(std::string& device_name, std::string& device_type, std::string& device_id) {
    ESP_LOGI(TAG, "starting test services...");

    #if defined(CONFIG_FIRMWARE_USE_AMP)
    ESP_LOGI(TAG, "+ IR control");
    ir.start();
    ESP_LOGI(TAG, "+ OLED display");
    oled.start();
    #endif

    #if defined(CONFIG_FIRMWARE_USE_TEMPERATURE_SI7021) || defined(CONFIG_FIRMWARE_USE_TEMPERATURE_SI7021)
    temperature.start(device_name);
    ESP_LOGI(TAG, "+ Temperature");
    #endif

    #if defined(CONFIG_FIRMWARE_USE_AWS)
    ESP_LOGI(TAG, "+ AWS");
    aws.start(device_name);
    #endif

    #if defined(CONFIG_FIRMWARE_USE_HOMEKIT)
    ESP_LOGI(TAG, "+ Homekit");
    homekit.start(device_name, device_type, device_id);
    #endif
}

void setup_sensor(std::string& device_name, std::string& device_type, std::string& device_id) {
    ESP_LOGI(TAG, "starting sensor services...");

    #if defined(CONFIG_FIRMWARE_USE_TEMPERATURE_SI7021) || defined(CONFIG_FIRMWARE_USE_TEMPERATURE_SI7021)
    temperature.start(device_name);
    ESP_LOGI(TAG, "+ Temperature");
    #endif

    #if defined(CONFIG_FIRMWARE_USE_AWS)
    ESP_LOGI(TAG, "+ AWS");
    aws.start(device_name);
    #endif

    #if defined(CONFIG_FIRMWARE_USE_HOMEKIT)
    ESP_LOGI(TAG, "+ Homekit");
    homekit.start(device_name, device_type, device_id);
    #endif
}


void setup_amp(std::string& device_name, std::string& device_type, std::string& device_id) {
    ESP_LOGI(TAG, "starting amp services...");

    #if defined(CONFIG_FIRMWARE_USE_AMP)
    ESP_LOGI(TAG, "+ Amp controller");
    amp.start();
    ESP_LOGI(TAG, "+ IR control");
    ir.start();
    ESP_LOGI(TAG, "+ OLED display");
    oled.start();
    #endif

    #if defined(CONFIG_FIRMWARE_USE_HOMEKIT)
    ESP_LOGI(TAG, "+ Homekit");
    homekit.start(device_name, device_type, device_id);
    #endif
}



void setup_wifi() {
    tcpip_adapter_init();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config;
    wifi_sta_config_t sta_config;

    #if defined(CONFIG_FIRMWARE_HARDCODE_WIFI)
    strcpy((char *)sta_config.ssid, FIRMWARE_WIFI_SSID);
    strcpy((char *)sta_config.password, FIRMWARE_WIFI_PASSWORD);
    #else
    std::string ssid;
    std::string password;

    if (!get_kv(WIFI_SSID_KEY, ssid) || !get_kv(WIFI_PASSWORD_KEY, password)) {
        ESP_LOGE(TAG, "no wifi credentials found, connect to the console and run 'save-wifi'");
        return;
    }

    strcpy((char *)sta_config.ssid, ssid.c_str());
    strcpy((char *)sta_config.password, password.c_str());
    #endif

    sta_config.bssid_set = false;
    sta_config.channel = 0;
    sta_config.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta = sta_config;
    
    wifi_country_t country;

    strcpy((char *) country.cc, "US");
    country.policy = WIFI_COUNTRY_POLICY_MANUAL;
    country.schan = 1;
    country.nchan = 13;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_country(&country));
    ESP_ERROR_CHECK(esp_wifi_start());
}


void setup_device(std::string& device_id) {
    std::string device_name;
    std::string device_type;

    bool provisioned = false;

    if (get_kv(DEVICE_PROVISIONING_NAME_KEY, device_name) && get_kv(DEVICE_PROVISIONING_TYPE_KEY, device_type)) {
        provisioned = true;
    } else {
        ESP_LOGE( TAG, "not provisioned, use the web interface or console to set up device");
        device_type = "none";
        device_name = "none";
    }

    ESP_LOGI(TAG, "provisioned %s:%s", device_name.c_str(), device_type.c_str());
    
    led.start();

    #if defined(CONFIG_FIRMWARE_USE_OTA)
    updater.start(device_name);
    #endif

    #if defined(CONFIG_FIRMWARE_USE_CONSOLE)
    console.start();
    #endif

    #if defined(CONFIG_FIRMWARE_USE_WEB)
    web.start(device_name, device_type);
    #endif

    if (provisioned) {
        if (device_type == "amp") {
            setup_amp(device_name, device_type, device_id);
        } else if (device_type == "sensor") {
            setup_sensor(device_name, device_type, device_id);
        } else if (device_type == "test") {
            setup_test(device_name, device_type, device_id);
        }
        xEventGroupSetBits(provisioning_event_group, PROVISIONED_BIT);
    }
}



static void device_setup_task(void *param) {
    static const char *_TAG = "[Setup]";

    ESP_LOGI(_TAG, "setup task running, waiting for wifi");

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

    ESP_LOGI(_TAG, "setup task continuing");

    uint8_t mac[6];
    esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
    char ser[32] = {0,};
    sprintf(ser, "%02X-%02X-%02X-%02X-%02X-%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    std::string device_id(ser);

    setup_device(device_id);

    xEventGroupSetBits(provisioning_event_group, PROVISIONED_BIT);

    ESP_LOGD(_TAG, "setup task done");

    vTaskDelete(NULL);
}


static void stats_task(void *param) {
    static const char *_TAG = "[Stat]";

    while (true) {

        size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        size_t min_free_heap = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);

        ESP_LOGI(_TAG, "free: %d, min: %d", free_heap, min_free_heap);

        vTaskDelay(5000 / portTICK_RATE_MS);
    }
}


/**
 * Entry point
 *
 */

extern "C" {
void app_main();
}


void app_main() {

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    wifi_event_group = xEventGroupCreate();
    provisioning_event_group = xEventGroupCreate();


    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, nullptr));


    setup_queues();

    setup_wifi();

    xTaskCreate(&stats_task, "stats_task", 3072, NULL, (tskIDLE_PRIORITY + 10), &stats_task_handle);

    xTaskCreate(&device_setup_task, "device_setup_task", 8192, NULL, (tskIDLE_PRIORITY + 10), &setup_task_handle);
}
