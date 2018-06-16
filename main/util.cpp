#include "pch.hpp"

#include "util.hpp"

static const char *TAG = "[Util]";


/*
 * @brief Time handling
 */

unsigned long IRAM_ATTR millis() {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

double round(double d) {
    return floor(d + 0.5);
}

long map(int input, int input_start, int input_end, int output_start, int output_end) {
    double slope = 1.0 * (output_end - output_start) / (input_end - input_start);
    long output = output_start + round(slope * (input - input_start));

    return output;
}
/*
 * @brief Volume control messages
 */

bool volume_up() {
    IPCMessage volume_message;
    volume_message.messageType = ControlMessageTypeVolumeUp;

    if (!xQueueOverwrite(volumeChangeQueue, (void *)&volume_message)) {
        ESP_LOGE(TAG, "failed to send volume up on volumeChangeQueue");
        return false;
    }
    return true;
}


bool volume_down() {
    IPCMessage volume_message;
    volume_message.messageType = ControlMessageTypeVolumeDown;

    if (!xQueueOverwrite(volumeChangeQueue, (void *)&volume_message)) {
        ESP_LOGE(TAG, "failed to send volume down on volumeChangeQueue");
        return false;
    }
    return true;
}


bool volume_set(uint8_t level) {
    IPCMessage volume_message;
    volume_message.messageType = ControlMessageTypeVolumeSet;
    volume_message.volumeLevel = level;

    if (xQueueOverwrite(volumeChangeQueue, (void *)&volume_message)) {
        ESP_LOGE(TAG, "failed to send volume set on volumeChangeQueue");
        return false;
    }
    return true;
}


bool signal_identify_on_device(bool state) {
    IPCMessage message;
    message.messageType = EventIdentifyLED;
    message.led_state = state;

    if (xQueueOverwrite(ledQueue, (void *)&message)) {
        ESP_LOGE(TAG, "failed to send identify message on ledQueue");
        return false;
    }
    return true;
}

bool signal_error_on_device(bool state) {
    IPCMessage message;
    message.messageType = EventErrorLED;
    message.led_state = state;

    if (xQueueOverwrite(ledQueue, (void *)&message)) {
        ESP_LOGE(TAG, "failed to send error message on ledQueue");
        return false;
    }
    return true;
}

bool signal_ready_on_device(bool state) {
    IPCMessage message;
    message.messageType = EventReadyLED;
    message.led_state = state;

    if (xQueueOverwrite(ledQueue, (void *)&message)) {
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

const char* error_string(esp_err_t err) {
    const char* msg;
    switch (err) {
        case ESP_OK: //OTA operation commenced successfully.
            msg = "ESP_OK";
            break;
        case ESP_ERR_INVALID_ARG: //partition or out_handle arguments were NULL, or partition doesn't point to an OTA app partition.
            msg = "ESP_ERR_INVALID_ARG";
            break;
        case ESP_ERR_NO_MEM: //Cannot allocate memory for OTA operation.
            msg = "ESP_ERR_NO_MEM";
            break;
        case ESP_ERR_OTA_PARTITION_CONFLICT: //Partition holds the currently running firmware, cannot update in place.
            msg = "ESP_ERR_OTA_PARTITION_CONFLICT";
            break;
        case ESP_ERR_NOT_FOUND: //Partition argument not found in partition table.
            msg = "ESP_ERR_NOT_FOUND";
            break;
        case ESP_ERR_OTA_SELECT_INFO_INVALID: //The OTA data partition contains invalid data.
            msg = "ESP_ERR_OTA_SELECT_INFO_INVALID";
            break;
        case ESP_ERR_INVALID_SIZE: //Partition doesn't fit in configured flash size.
            msg = "ESP_ERR_INVALID_SIZE";
            break;
        case ESP_ERR_FLASH_OP_TIMEOUT:
            msg = "ESP_ERR_FLASH_OP_TIMEOUT";
            break;
        case ESP_ERR_FLASH_OP_FAIL: //Flash write failed.
            msg = "ESP_ERR_FLASH_OP_FAIL";
            break;
        default:
            msg = "Unknown error";
            break;
    }
    return msg;
}
