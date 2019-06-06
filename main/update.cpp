#include "pch.hpp"


#if defined(CONFIG_FIRMWARE_USE_OTA)

#include "update.hpp"

#include <Semver.hpp>
#include <JSON.h>

static const char *TAG = "[Update]";


extern const char letsencrypt_chain_pem_start[] asm("_binary_letsencrypt_chain_pem_start");
extern const char letsencrypt_chain_pem_end[] asm("_binary_letsencrypt_chain_pem_end");


/**
 * @brief Update
 *
 */

Update::Update(const char* update_manifest_url, std::string& device_name): 
_device_name(device_name), 
_update_url(update_manifest_url) {

    esp_err_t err;

    if (configured != running) {
        ESP_LOGW(
            TAG, "configured 0x%08x, but running 0x%08x", configured->address, running->address);
    }
    ESP_LOGD(TAG, "Running partition type %d subtype %d (offset 0x%08x)", running->type,
        running->subtype, running->address);

    if (!update_partition) {
        throw std::runtime_error("failed to get update_partition");
    }

    ESP_LOGD(TAG, "next ota partition subtype %d at offset 0x%x", update_partition->subtype, update_partition->address);


    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &_update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        throw std::runtime_error("esp_ota_begin failed");
    }
 }


Update::~Update() {
    esp_err_t err;

    err = esp_ota_end(_update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "updating OTA boot partition");

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "boot select error: %s", esp_err_to_name(err));
    }   

    ESP_LOGI(TAG, "update successful, restarting");
    esp_restart();
};

/**
 *
 * @brief Update handler
 */

void Update::check() {
    esp_err_t err;

    /* an image total length */
    int binary_file_length = 0;


    long timeout = 5000;


    char user_agent[32];
    snprintf(user_agent, sizeof(user_agent), "%s/%s", this->_device_name.c_str(), FIRMWARE_VERSION);



    HTTPSClient http_client(user_agent, letsencrypt_chain_pem_start, timeout);

    std::string available_version_s;
    std::string firmware_url;

    int res;
    try {
        memset(this->_text, 0, TEXT_BUFFSIZE);

        http_client.set_read_cb([&] (const char* buf, int length) {
            memcpy(_text, buf, length);
        });

        res = http_client.get(_update_url);

        if (res >= 500) {
            throw std::runtime_error("server error");
        } else if (res == 404) {
            throw std::runtime_error("no update manifest found");
        } else if (res == 200) {
            ESP_LOGI(TAG, "received update manifest");
        } else {
            ESP_LOGE(TAG, "unknown manifest request error: %d", res);
            throw std::runtime_error("HTTP failed");
        }
        
        ESP_LOGD(TAG, "http request success: %d", res);

        std::string body(_text);

        auto m = JSON::parseObject(body);

        available_version_s = m.getString("available_version");
        firmware_url = m.getString("firmware_url");

        JSON::deleteObject(m);

        if (available_version_s.length() == 0) {
            ESP_LOGE(TAG, "available_version key missing");
            throw std::runtime_error("available_version key missing");
        }
        if (firmware_url.length() == 0) {
            ESP_LOGE(TAG, "firmware_url key missing");
            throw std::runtime_error("firmware_url key missing");
        }
    } catch (std::exception &ex) {
        ESP_LOGE(TAG, "manifest download failed: %s", ex.what());
        throw std::runtime_error("manifest download failed");
    }


    try {
        ESP_LOGI(TAG, "current git version: %s", FIRMWARE_VERSION);

        Semver current_version(FIRMWARE_VERSION);
        ESP_LOGI(TAG, "current version: %s", current_version.string().c_str());

        Semver available_version(available_version_s);

        ESP_LOGI(TAG, "found latest update: %s", available_version.string().c_str());

        if (available_version <= current_version) {
            throw std::runtime_error("no update available");
        }
    } catch (std::exception &ex) {
        ESP_LOGE(TAG, "version check failed: %s", ex.what());
        throw std::runtime_error("version check failed");
    }

    ESP_LOGI(TAG, "beginning firmware upgrade");

    try {
        http_client.set_read_cb([&] (const char* buf, int length) {
            err = esp_ota_write(_update_handle, buf, length);
            if (err != ESP_OK) {
                ESP_LOGD(TAG, "esp_ota_write: %s", esp_err_to_name(err));
            }
            binary_file_length += length;
            ESP_LOGD(TAG, "written %d", binary_file_length);
        });

        ESP_LOGI(TAG, "requesting firmware binary from server: %s", firmware_url.c_str());

        res = http_client.get(firmware_url.c_str());
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
        ESP_LOGE(TAG, "firmware download failed: %s", ex.what());
    }

    ESP_LOGI(TAG, "finished writing firmware to flash, closing OTA session");
}

#endif
