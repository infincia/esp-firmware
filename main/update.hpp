#pragma once

#include "pch.hpp"

#if defined(CONFIG_FIRMWARE_USE_OTA)

#ifndef UPDATE_H_
#define UPDATE_H_

#define TEXT_BUFFSIZE 1024


class Update {
public:
	Update(const char* update_manifest_url, std::string& device_name);
	virtual ~Update();
	void check();

private:
    esp_ota_handle_t _update_handle;
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(running);

	std::string _device_name;

    const char* _update_url;
    
    /* an packet receive buffer */
    char _text[TEXT_BUFFSIZE + 1] = {0};
};

#endif /* UPDATE_H_ */

#endif /* USE_OTA */
