#pragma once

#include "pch.hpp"

#if defined(CONFIG_FIRMWARE_USE_OTA)

#ifndef UPDATE_H_
#define UPDATE_H_

#define TEXT_BUFFSIZE 1024

class Update {
public:
	Update(const char* update_manifest_url);
	virtual ~Update();
	void task();
    void start(std::string& device_name, std::string& device_type, std::string& device_id);
private:
	bool update(const char* url);

	TaskHandle_t update_task_handle;
    long failure_count = 0;

  

	std::string device_name;
	std::string device_type;
    std::string device_id;

    const char* update_url;
    
    /* an packet receive buffer */
    char text[TEXT_BUFFSIZE + 1] = {0};
};

#endif /* UPDATE_H_ */

#endif /* USE_OTA */
