#include "pch.hpp"

#include "util.hpp"

static const char *TAG = "[Util]";


/*
 * @brief Time handling
 */

unsigned long IRAM_ATTR millis() {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}


/*
 * @brief Volume control messages
 */

bool volume_up() {
    VolumeControlMessage volume_message;
    volume_message.messageType = ControlMessageTypeVolumeUp;

    if (xQueueSendToFront(volumeChangeQueue, (void *)&volume_message, (TickType_t)10) != pdPASS) {
        ESP_LOGE(TAG, "failed to send volume up on volumeChangeQueue");
        return false;
    }
    return true;
}


bool volume_down() {
    VolumeControlMessage volume_message;
    volume_message.messageType = ControlMessageTypeVolumeDown;

    if (xQueueSendToFront(volumeChangeQueue, (void *)&volume_message, (TickType_t)10) != pdPASS) {
        ESP_LOGE(TAG, "failed to send volume down on volumeChangeQueue");
        return false;
    }
    return true;
}


bool volume_set(uint8_t level) {
    VolumeControlMessage volume_message;
    volume_message.messageType = ControlMessageTypeVolumeSet;
    volume_message.volumeLevel = level;

    if (xQueueSendToFront(volumeChangeQueue, (void *)&volume_message, (TickType_t)10) != pdPASS) {
        ESP_LOGE(TAG, "failed to send volume set on volumeChangeQueue");
        return false;
    }
    return true;
}


bool signal_identify_on_device(bool state) {
    LEDMessage message;
    message.messageType = EventIdentifyLED;
    message.state = state;

    if (xQueueSendToFront(ledQueue, (void *)&message, (TickType_t)10) != pdPASS) {
        ESP_LOGE(TAG, "failed to send identify message on ledQueue");
        return false;
    }
    return true;
}

bool signal_error_on_device(bool state) {
    LEDMessage message;
    message.messageType = EventErrorLED;
    message.state = state;

    if (xQueueSendToFront(ledQueue, (void *)&message, (TickType_t)10) != pdPASS) {
        ESP_LOGE(TAG, "failed to send error message on ledQueue");
        return false;
    }
    return true;
}

bool signal_ready_on_device(bool state) {
    LEDMessage message;
    message.messageType = EventReadyLED;
    message.state = state;

    if (xQueueSendToFront(ledQueue, (void *)&message, (TickType_t)10) != pdPASS) {
        ESP_LOGE(TAG, "failed to send ready message on ledQueue");
        return false;
    }
    return true;
}
/**
 *
 * @brief Load/save value on flash
 */

bool get_kv(const char* key, std::string &value) {

    esp_err_t err;
    size_t required_size;
    nvs_handle my_handle;

    err = nvs_open("fw", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        return false;
    } 

    err = nvs_get_str(my_handle, key, NULL, &required_size);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "get_kv error %d", err);
        return false;
    } 

    char* _value = (char*)malloc(required_size);

    err = nvs_get_str(my_handle, key, _value, &required_size);

    nvs_close(my_handle);

    value = std::string(_value);

    return true;
}

bool set_kv(const char* key, std::string value) {

    esp_err_t err;
    nvs_handle my_handle;

    err = nvs_open("fw", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        return false;
    } 

    err = nvs_set_str(my_handle, key, value.c_str());

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "set_kv error %d", err);
        return false;
    } 

    err = nvs_commit(my_handle);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "set_kv error %d", err);
        return false;
    } 

    nvs_close(my_handle);

    return true;
}



bool set_kv(const char* key, int32_t value) {

    esp_err_t err;
    nvs_handle my_handle;

    err = nvs_open("fw", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "set_kv error %d", err);
        return false;
    } 

    err = nvs_set_i32(my_handle, key, value);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "set_kv error %d", err);
        return false;
    } 

    err = nvs_commit(my_handle);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "set_kv error %d", err);
        return false;
    } 

    nvs_close(my_handle);

    return true;
}

bool get_kv(const char* key, int32_t* value) {
    esp_err_t err;
    nvs_handle my_handle;

    err = nvs_open("fw", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        return false;
    } 

    err = nvs_get_i32(my_handle, key, value);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "get_kv error %d", err);
        return false;
    } 

    nvs_close(my_handle);

    return true;
}