#include "../pch.hpp"

#if defined(CONFIG_FIRMWARE_USE_HOMEKIT)

#include <string>

#include "homekit.hpp"

#include "hap.h"

static const char *TAG = "[Homekit]";

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))


/**
 *
 * @brief Homekit task wrapper
 */

static void task_wrapper(void *param) {
    auto *instance = static_cast<Homekit *>(param);
    instance->task();
}

static void* a;

static SemaphoreHandle_t ev_mutex;

static void* _temperature_ev_handle = NULL;
static void* _humidity_ev_handle =  NULL;
static void* _volume_ev_handle =  NULL;
static void* _mute_ev_handle =  NULL;

static int h_temperature = 0;
static int h_humidity = 0;
static int h_volume = 0;
static int h_mute = 0;

void* identify_read(void* arg)
{
    return (void*)true;
}

Homekit::Homekit() { }


Homekit::~Homekit() = default;


void Homekit::start(std::string& device_name, std::string& device_type, std::string& device_id) {

    ESP_LOGD(TAG, "start");

    this->device_name = device_name;
    this->device_type = device_type;
    this->device_id = device_id;
    
    xTaskCreate(&task_wrapper, "homekit_task", 16384, this, (tskIDLE_PRIORITY + 10), &this->homekit_task_handle);
}


static void* _temperature_read(void* arg)
{
    ESP_LOGI(TAG, "_temperature_read");
    return (void*)h_temperature;
}

void _temperature_notify(void* arg, void* ev_handle, bool enable)
{
    ESP_LOGI(TAG, "_temperature_notify");
    //xSemaphoreTake(ev_mutex, 0);

    if (enable) 
        _temperature_ev_handle = ev_handle;
    else 
        _temperature_ev_handle = NULL;

    //xSemaphoreGive(ev_mutex);
}

static void* _humidity_read(void* arg)
{
    ESP_LOGI(TAG, "_humidity_read");
    return (void*)h_humidity;
}

void _humidity_notify(void* arg, void* ev_handle, bool enable)
{
    ESP_LOGI(TAG, "_humidity_notify");
    //xSemaphoreTake(ev_mutex, 0);

    if (enable) 
        _humidity_ev_handle = ev_handle;
    else 
        _humidity_ev_handle = NULL;

    //xSemaphoreGive(ev_mutex);
}



static void* _mute_read(void* arg) {
    ESP_LOGI(TAG, "_mute_read");
    
    return (void*)h_mute;
}

static void _mute_write(void* arg, void* value, int value_len) {
    ESP_LOGI(TAG, "_mute_write");    
    h_mute = (int)value;
}

void _mute_notify(void* arg, void* ev_handle, bool enable) {
    ESP_LOGI(TAG, "_mute_notify");
    //xSemaphoreTake(ev_mutex, 0);

    if (enable) {
        _mute_ev_handle = ev_handle;
    } else { 
        _mute_ev_handle = NULL;
    }

    //xSemaphoreGive(ev_mutex);
}


static void* _volume_read(void* arg) {
    ESP_LOGI(TAG, "_volume_read");

    return (void*)h_volume;
}

static void _volume_write(void* arg, void* value, int value_len) {
    ESP_LOGI(TAG, "_volume_write");
    int t = (int)value;

    long v = map(t, 0, 100, 0, 63);

    h_volume = static_cast<int>(v);

    volume_set(h_volume);
}

void _volume_notify(void* arg, void* ev_handle, bool enable) {
    ESP_LOGI(TAG, "_volume_notify");
    //xSemaphoreTake(ev_mutex, 0);

    if (enable) {
        _volume_ev_handle = ev_handle;
    } else { 
        _volume_ev_handle = NULL;
    }

    //xSemaphoreGive(ev_mutex);
}


void hap_object_init(void* arg) {
    auto *instance = static_cast<Homekit *>(arg);

    void* accessory_object = hap_accessory_add(a);
    struct hap_characteristic cs[] = {
        {HAP_CHARACTER_IDENTIFY, (void*)true, instance, identify_read, NULL, NULL},
        {HAP_CHARACTER_MANUFACTURER, (void*)"Infincia LLC", instance, NULL, NULL, NULL},
        {HAP_CHARACTER_MODEL, (void*)instance->device_type.c_str(), instance, NULL, NULL, NULL},
        {HAP_CHARACTER_NAME, (void*)instance->device_name.c_str(), instance, NULL, NULL, NULL},
        {HAP_CHARACTER_SERIAL_NUMBER, (void*)instance->device_id.c_str(), instance, NULL, NULL, NULL},
        {HAP_CHARACTER_FIRMWARE_REVISION, (void*)VERSION, instance, NULL, NULL, NULL},
    };
    hap_service_and_characteristics_add(a, accessory_object, HAP_SERVICE_ACCESSORY_INFORMATION, cs, ARRAY_SIZE(cs));

    if (instance->device_type == "sensor") {
        struct hap_characteristic humidity_sensor[] = {
            {HAP_CHARACTER_CURRENT_RELATIVE_HUMIDITY, (void*)h_humidity, NULL, _humidity_read, NULL, _humidity_notify},
            {HAP_CHARACTER_NAME, (void*)"Humidity" , NULL, NULL, NULL, NULL},
        };
        hap_service_and_characteristics_add(a, accessory_object, HAP_SERVICE_HUMIDITY_SENSOR, humidity_sensor, ARRAY_SIZE(humidity_sensor));

        struct hap_characteristic temperature_sensor[] = {
            {HAP_CHARACTER_CURRENT_TEMPERATURE, (void*)h_temperature, NULL, _temperature_read, NULL, _temperature_notify},
            {HAP_CHARACTER_NAME, (void*)"Temperature" , NULL, NULL, NULL, NULL},
        };
        hap_service_and_characteristics_add(a, accessory_object, HAP_SERVICE_TEMPERATURE_SENSOR, temperature_sensor, ARRAY_SIZE(temperature_sensor));
    } else if (instance->device_type == "amp") {
        struct hap_characteristic speaker[] = {
            {HAP_CHARACTER_VOLUME, (void*)h_volume, NULL, _volume_read, _volume_write, _volume_notify},
            {HAP_CHARACTER_MUTE, (void*)h_mute, NULL, _mute_read, _mute_write, _mute_notify},
            {HAP_CHARACTER_NAME, (void*)"Volume" , NULL, NULL, NULL, NULL},
        };
        hap_service_and_characteristics_add(a, accessory_object, HAP_SERVICE_SPEAKER, speaker, ARRAY_SIZE(speaker));
    }
}

/**
 *
 * @brief Task loop
 */

void Homekit::task() {
    ESP_LOGI(TAG, "running");
    
    vSemaphoreCreateBinary(ev_mutex);

    /* Wait for wifi to be available */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

    ESP_LOGI(TAG, "HAP task wifi");

    /* Wait for device provisioning */
    xEventGroupWaitBits(provisioning_event_group, PROVISIONED_BIT, false, true, portMAX_DELAY);
    
    ESP_LOGI(TAG, "HAP task provisioned");


    // load PIN from kv store, or generate and store a new one
    if (!get_kv(HOMEKIT_PIN_KEY, pin)) {
        uint8_t first = esp_random() % 999;
        uint8_t second = esp_random() % 99;
        uint8_t third = esp_random() % 999;


        char _pin[11];
        snprintf(_pin, sizeof(_pin), "%03d-%02d-%03d", first, second, third);

        pin = _pin;

        ESP_LOGI(TAG, "generated a new pin: %s", _pin);

        if (!set_kv(HOMEKIT_PIN_KEY, pin)) {
            ESP_LOGE(TAG, "pin could not be saved");
            abort();
        }
    }


    hap_init();

    ESP_LOGI(TAG, "HAP init");


    hap_accessory_callback_t callback;
    callback.hap_object_init = hap_object_init;
    a = hap_accessory_register((char*)this->device_name.c_str(), (char*)this->device_id.c_str(), (char*)pin.c_str(), (char*)"Infincia LLC", HAP_ACCESSORY_CATEGORY_SENSOR, 811, 1, this, &callback);

    ESP_LOGI(TAG, "HAP loop running");

    while (true) {
        IPCMessage message;

        if (xQueueReceive(homekitQueue, &(message), (TickType_t)10)) {
            if (message.messageType == EventTemperatureSensorValue) {
                ESP_LOGD(TAG, "temperature update received: %f", message.temperature);

                //xSemaphoreTake(ev_mutex, 0);

                this->temperature = message.temperature;
                this->humidity = message.humidity;

                h_temperature = this->temperature * 100;
                h_humidity = this->humidity * 100;

                if (_humidity_ev_handle) {
                    hap_event_response(a, _humidity_ev_handle, (void*)h_humidity);
                }

                if (_temperature_ev_handle) {
                    hap_event_response(a, _temperature_ev_handle, (void*)h_temperature);
                }
                //xSemaphoreGive(ev_mutex);

            } else if (message.messageType == ControlMessageTypeVolumeSet) {
                ESP_LOGD(TAG, "volume update received: %d", message.volumeLevel);

                //xSemaphoreTake(ev_mutex, 0);

                long v = map(message.volumeLevel, 0, 63, 0, 100);

                this->volume  = static_cast<int>(v);

                h_volume = this->volume;

                if (_volume_ev_handle) {
                    hap_event_response(a, _volume_ev_handle, (void*)h_volume);
                }

                //xSemaphoreGive(ev_mutex);
            }
        }
    }
}

#endif
