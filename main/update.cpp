#include "pch.hpp"


#if defined(CONFIG_FIRMWARE_USE_OTA)

#include "update.hpp"

#include <Semver.hpp>
#include <JSON.h>

static const char *TAG = "[Update]";


extern const char letsencrypt_chain_pem_start[] asm("_binary_letsencrypt_chain_pem_start");
extern const char letsencrypt_chain_pem_end[] asm("_binary_letsencrypt_chain_pem_end");


/**
 * @brief Task wrapper
 */

static void task_wrapper(void *param) {
	auto *instance = static_cast<Update *>(param);
    instance->task();
}


/**
 * @brief Update
 *
 */

Update::Update(const char* update_manifest_url): update_url(update_manifest_url) { }


Update::~Update() = default;


void Update::start(std::string& device_name, std::string& device_type, std::string& device_id) {
    this->device_name = device_name;
    this->device_type = device_type;
    this->device_id = device_id;

    ESP_LOGI(TAG, "start");

    xTaskCreate(&task_wrapper, "update_task", 8192, this, (tskIDLE_PRIORITY + 10),
        &this->update_task_handle);

}


/**
 *
 * @brief Update handler
 */

bool Update::update(const char* url) {
    esp_err_t err;
    esp_ota_handle_t update_handle;

    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(running);

    /* an image total length */
    int binary_file_length = 0;

    if (configured != running) {
        ESP_LOGW(
            TAG, "configured 0x%08x, but running 0x%08x", configured->address, running->address);
    }
    ESP_LOGD(TAG, "Running partition type %d subtype %d (offset 0x%08x)", running->type,
        running->subtype, running->address);

    if (!update_partition) {
        ESP_LOGE(TAG, "failed to get update partition");
        return false;
    }

    ESP_LOGD(TAG, "next ota partition subtype %d at offset 0x%x", update_partition->subtype, update_partition->address);


    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK) {
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
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", msg);
        return false;
    }

    long timeout = 5000;


    ESP_LOGI(TAG, "starting version check from %s", VERSION);
    char user_agent[32];
    snprintf(user_agent, sizeof(user_agent), "%s/%s", this->device_name.c_str(), VERSION);



    HTTPSClient http_manifest(user_agent, letsencrypt_chain_pem_start, timeout);

    std::string available_version_s;
    std::string firmware_url;

    int res;
    try {
        memset(text, 0, TEXT_BUFFSIZE);

        http_manifest.set_read_cb([&] (const char* buf, int length) {
            memcpy(text, buf, length);
        });

        res = http_manifest.get(url);

        if (res >= 500) {
            throw std::runtime_error("server error");
        } else if (res == 404) {
            throw std::runtime_error("no update manifest found");
        }
        
        std::string body(text);

        auto m = JSON::parseObject(body);

        available_version_s = m.getString("available_version");
        firmware_url = m.getString("firmware_url");

        JSON::deleteObject(m);

        if (available_version_s.length() == 0) {
            ESP_LOGE(TAG, "available_version key missing");
            return false;
        }
        if (firmware_url.length() == 0) {
            ESP_LOGE(TAG, "firmware_url key missing");
            return false;
        }
    } catch (std::exception &ex) {
        ESP_LOGE(TAG, "update failed: %s", ex.what());
        return false;
    }


    try {
        Semver current_version(VERSION);
        Semver available_version(available_version_s);

        if (available_version <= current_version) {
            ESP_LOGI(TAG, "no update available");
            return false;
        }
    } catch (std::exception &ex) {
        ESP_LOGE(TAG, "version check failed: %s", ex.what());
        return false;    
    }


    HTTPSClient http(user_agent, letsencrypt_chain_pem_start, timeout);

    try {
        http.set_read_cb([&] (const char* buf, int length) {
            err = esp_ota_write(update_handle, buf, length);
            if (err != ESP_OK) {
                ESP_LOGD(TAG, "esp_ota_write: %d", err);
            }
            binary_file_length += length;
            ESP_LOGD(TAG, "written %d", binary_file_length);
        });

        res = http.get(firmware_url.c_str());
        ESP_LOGD(TAG, "http request success: %d", res);

        if (res >= 500) {
            throw std::runtime_error("server error");
        } else if (res == 404) {
            throw std::runtime_error("firmware binary not found");
        } else if (res == 200) {
            ESP_LOGD(TAG, "firmware size: %d", binary_file_length);
        } else {
            ESP_LOGE(TAG, "unknown error: %d", res);
            throw std::runtime_error("HTTP failed");
        }

    } catch (std::exception &ex) {
        ESP_LOGE(TAG, "update failed: %s", ex.what());
    }

    if (ESP_OK != esp_ota_end(update_handle)) {
        ESP_LOGE(TAG, "esp_ota_end failed");
        return false;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "boot select error: %d", err);
        return false;
    }

    return true;
}


/**
 *
 * @brief Task loop
 */

void Update::task() {
    ESP_LOGI(TAG, "running");

    /* Wait for wifi to be available */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

    // wait 60s before attempting first update check at boot
    vTaskDelay(60000 / portTICK_RATE_MS);


    while (true) {
        ESP_LOGI(TAG, "checking for update");

        if (this->update(update_url)) {
            ESP_LOGI(TAG, "updated, restarting");
            esp_restart();
        }

        vTaskDelay(90000 / portTICK_RATE_MS);
    }
}

#endif
