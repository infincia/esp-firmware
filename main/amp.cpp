#include "pch.hpp"


#if defined(USE_AMP)


#include "amp.hpp"

#include <max9744_espidf.h>

static const char *TAG = "[Amp]";



/**
 *
 * @brief Amp task wrapper
 */

static void task_wrapper(void *param) {
    auto *instance = static_cast<Amp *>(param);
    instance->task();
}


Amp::Amp() : current_volume(MIN_VOLUME), use_spread_spectrum(false) {
    max9744_init();

    xTaskCreate(&task_wrapper, "amp_task", 2048, this, (tskIDLE_PRIORITY + 10), &this->amp_task_handle);
}


Amp::~Amp() = default;


/**
 * @brief Volume control
 */

bool Amp::set_volume(uint8_t volume) {
    // Ensure the new value is within bounds

    if (volume >= MAX_VOLUME) {
        volume = MAX_VOLUME;
    }
    if (volume <= MIN_VOLUME) {
        volume = MIN_VOLUME;
    }

    current_volume = volume;

    int ret = max9744_set_volume(current_volume);
    if (ret == ESP_OK) {
        if (!set_kv(VOLUME_LEVEL_KEY, current_volume)) {
            ESP_LOGW(TAG, "volume level could not be saved");
        }

        char vols[5];
        sprintf(vols, "%d", current_volume);

        DisplayControlMessage message1;
        message1.messageType = ControlMessageTypeDisplayText;
        strcpy(message1.text, vols);

        if (!xQueueSend(displayQueue, (void *)&message1, (TickType_t)0)) {
            ESP_LOGE(TAG, "Setting display text failed");
        }

#if defined(USE_WEB)
        WebControlMessage message2;
        message2.messageType = ControlMessageTypeVolumeEvent;
        message2.volumeLevel = current_volume;

        if (!xQueueSend(webQueue, (void *)&message2, (TickType_t)0)) {
            ESP_LOGE(TAG, "Sending web volume event failed");
        }
#endif
    }
    return (ret == ESP_OK);
}


bool Amp::increase_volume() {
    if (current_volume >= MAX_VOLUME) {
        return false;
    }
    current_volume++;
    return this->set_volume(current_volume);
}


bool Amp::decrease_volume() {
    if (current_volume <= MIN_VOLUME) {
        return false;
    }
    current_volume--;
    return this->set_volume(current_volume);
}


/**
 *
 * @brief Task loop
 */

void Amp::task() {
    int32_t saved_volume;

    if (get_kv(VOLUME_LEVEL_KEY, &saved_volume)) {
        ESP_LOGD(TAG, "loaded volume: %d", saved_volume);
        this->set_volume(saved_volume);
    } else {
        ESP_LOGE(TAG, "using default volume");
        this->set_volume(DEFAULT_VOLUME);
    }

    while (true) {
        VolumeControlMessage message;
        if (xQueueReceive(volumeChangeQueue, &(message), (TickType_t)10)) {
            if (message.messageType == ControlMessageTypeVolumeUp) {
                this->increase_volume();
            } else if (message.messageType == ControlMessageTypeVolumeDown) {
                this->decrease_volume();
            } else if (message.messageType == ControlMessageTypeVolumeSet) {
                this->set_volume(message.volumeLevel);
            } else {
                ESP_LOGW(TAG, "unknown message type received: %d", message.messageType);
            }
        }
        vTaskDelay(50 / portTICK_RATE_MS);
    }
}

#endif
