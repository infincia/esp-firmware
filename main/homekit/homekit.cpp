#include "../pch.hpp"

#if defined(CONFIG_FIRMWARE_USE_HOMEKIT)

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

static int h_temperature = 0;
static int h_humidity = 0;

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
}

/**
 *
 * @brief Task loop
 */

void Homekit::task() {
    ESP_LOGI(TAG, "HAP task started");
    
    vSemaphoreCreateBinary(ev_mutex);

    /* Wait for wifi to be available */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

    ESP_LOGI(TAG, "HAP task wifi");

    /* Wait for device provisioning */
    xEventGroupWaitBits(provisioning_event_group, PROVISIONED_BIT, false, true, portMAX_DELAY);
    
    ESP_LOGI(TAG, "HAP task provisioned");

    hap_init();

    ESP_LOGI(TAG, "HAP init");


    hap_accessory_callback_t callback;
    callback.hap_object_init = hap_object_init;
    a = hap_accessory_register((char*)this->device_name.c_str(), (char*)this->device_id.c_str(), (char*)"053-58-197", (char*)"Infincia LLC", HAP_ACCESSORY_CATEGORY_SENSOR, 811, 1, this, &callback);

    ESP_LOGI(TAG, "HAP loop running");

    while (true) {
        SensorMessage message;

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

            }
        }
    }
}

#endif
