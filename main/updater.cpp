#include "pch.hpp"


#if defined(CONFIG_FIRMWARE_USE_OTA)

#include "updater.hpp"
#include "update.hpp"

static const char *TAG = "[Updater]";


extern const char letsencrypt_chain_pem_start[] asm("_binary_letsencrypt_chain_pem_start");
extern const char letsencrypt_chain_pem_end[] asm("_binary_letsencrypt_chain_pem_end");


/**
 * @brief Task wrapper
 */

static void task_wrapper(void *param) {
	auto *instance = static_cast<Updater *>(param);
    instance->task();
}


/**
 * @brief Updater
 *
 */

Updater::Updater(const char* update_manifest_url): update_url(update_manifest_url) { }


Updater::~Updater() = default;


void Updater::start(std::string& device_name) {
    this->device_name = device_name;

    ESP_LOGI(TAG, "start");

    xTaskCreate(&task_wrapper, "updater_task", 8192, this, (tskIDLE_PRIORITY + 10),
        &this->update_task_handle);

}


/**
 *
 * @brief Update handler
 */

void Updater::update() {
    try {
        Update i_update(update_url, device_name);
        i_update.check();
    } catch (std::exception &ex) {
        ESP_LOGE(TAG, "update failed: %s", ex.what());
    }
}


/**
 *
 * @brief Task loop
 */

void Updater::task() {
    ESP_LOGI(TAG, "running");

    /* Wait for wifi to be available */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

    // wait 10s before attempting first update check at boot
    vTaskDelay(10000 / portTICK_RATE_MS);


    while (true) {
        ESP_LOGV(TAG, "checking for update");

        this->update();

        vTaskDelay(60000 / portTICK_RATE_MS);
    }
}

#endif
