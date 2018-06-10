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

#if defined(CONFIG_FIRMWARE_USE_TEMPERATURE)
#include "temperature.hpp"
#endif

#if defined(CONFIG_FIRMWARE_USE_OTA)
#include "update.hpp"
#endif

#if defined(CONFIG_FIRMWARE_USE_AWS)
#include "aws/aws.hpp"
#endif

#if defined(CONFIG_FIRMWARE_USE_HOMEKIT)
#include "homekit/homekit.hpp"
#endif


static const char *TAG = "[Main]";

TaskHandle_t setup_task_handle;

/**
 * Tasks
 */

LED led;


#if defined(CONFIG_FIRMWARE_USE_CONSOLE)
Console console;
#endif

#if defined(CONFIG_FIRMWARE_USE_AMP)
Amp *amp;
IR *ir;
OLED oled;
#endif

#if defined(CONFIG_FIRMWARE_USE_TEMPERATURE)
Temperature temperature;
#endif

#if defined(CONFIG_FIRMWARE_USE_WEB)
Web *web;
#endif

#if defined(CONFIG_FIRMWARE_USE_OTA)
Update *update;
#endif

#if defined(CONFIG_FIRMWARE_USE_AWS)
AWS aws;
#endif

#if defined(CONFIG_FIRMWARE_USE_HOMEKIT)
Homekit *homekit;
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
            break;
        case SYSTEM_EVENT_STA_LOST_IP:
            ESP_LOGI(_TAG, "lost IP");
            // xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_STA_START:
            ESP_LOGI(_TAG, "STA start");
            ESP_ERROR_CHECK(esp_wifi_connect());
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            ESP_LOGI(_TAG, "STA connected");
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGI(_TAG, "STA disconnected");
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            // This is a workaround as ESP32 WiFi libs don't currently auto-reassociate.
            ESP_ERROR_CHECK(esp_wifi_connect());
            break;
        default:
            ESP_LOGI(_TAG, "event occurred, unknown type");
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
    volumeChangeQueue = xQueueCreate(3, sizeof(VolumeControlMessage));
    if (volumeChangeQueue == nullptr) {
        ESP_LOGE(TAG, "failed to create volume queue, restarting");
        abort();
    }

    displayQueue = xQueueCreate(2, sizeof(DisplayControlMessage));
    if (displayQueue == nullptr) {
        ESP_LOGE(TAG, "failed to create display queue, restarting");
        abort();
    }
    #endif

    #if defined(CONFIG_FIRMWARE_USE_WEB)
    webQueue = xQueueCreate(2, sizeof(WebControlMessage));
    if (webQueue == nullptr) {
        ESP_LOGE(TAG, "failed to create web queue, restarting");
        abort();
    }
    #endif

    #if defined(CONFIG_FIRMWARE_USE_AWS)
    awsQueue = xQueueCreate(2, sizeof(SensorMessage));
    if (awsQueue == nullptr) {
        ESP_LOGE(TAG, "failed to create aws queue, restarting");
        abort();
    }
    #endif

    #if defined(CONFIG_FIRMWARE_USE_HOMEKIT)
    homekitQueue = xQueueCreate(2, sizeof(SensorMessage));
    if (homekitQueue == nullptr) {
        ESP_LOGE(TAG, "failed to create homekit queue, restarting");
        abort();
    }
    #endif

    ledQueue = xQueueCreate(2, sizeof(LEDMessage));
    if (ledQueue == nullptr) {
        ESP_LOGE(TAG, "failed to create LED queue, restarting");
        abort();
    }
}




void setup_sensor(std::string& device_name, std::string& device_type, std::string& device_id) {
    ESP_LOGI(TAG, "starting sensor services...");

    #if defined(CONFIG_FIRMWARE_USE_TEMPERATURE)
    temperature.start();
    ESP_LOGI(TAG, "+ Temperature");
    #endif

    #if defined(CONFIG_FIRMWARE_USE_AWS)
    ESP_LOGI(TAG, "+ AWS");
    aws.start(device_name, device_type, device_id);
    #endif

    #if defined(CONFIG_FIRMWARE_USE_HOMEKIT)
    ESP_LOGI(TAG, "+ Homekit");
    homekit = new Homekit(device_name, device_type, device_id);
    #endif
}


void setup_amp(std::string& device_name, std::string& device_type, std::string& device_id) {
    ESP_LOGI(TAG, "starting amp services...");

    #if defined(CONFIG_FIRMWARE_USE_AMP)
    amp = new Amp();
    ir = new IR();
    oled.start();
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
    strcpy((char *)sta_config.ssid, CONFIG_FIRMWARE_WIFI_SSID);
    strcpy((char *)sta_config.password, CONFIG_FIRMWARE_WIFI_PASSWORD);
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

        #if defined(CONFIG_FIRMWARE_USE_WEB)
        web = new Web(CONFIG_FIRMWARE_WEB_PORT, device_name, device_type, device_id);
        #endif
    }

    ESP_LOGI(TAG, "provisioned %s:%s", device_name.c_str(), device_type.c_str());
    
    led.start();

    #if defined(CONFIG_FIRMWARE_USE_OTA)
    update = new Update(device_name, device_type, device_id);
    #endif

    #if defined(CONFIG_FIRMWARE_USE_CONSOLE)
    console.start();
    #endif


    if (provisioned) {
        if (device_type == "amp") {
            setup_amp(device_name, device_type, device_id);
        } else if (device_type == "sensor") {
            setup_sensor(device_name, device_type, device_id);
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
    sprintf(ser, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    std::string device_id(ser);

    setup_device(device_id);

    xEventGroupSetBits(provisioning_event_group, PROVISIONED_BIT);

    ESP_LOGD(_TAG, "setup task done");

    vTaskDelete(NULL);
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


    xTaskCreate(&device_setup_task, "device_setup_task", 8192, NULL, (tskIDLE_PRIORITY + 10), &setup_task_handle);
}
