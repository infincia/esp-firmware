#pragma once

#include "pch.hpp"

#if defined(USE_OTA)

#ifndef UPDATE_H_
#define UPDATE_H_

#define TEXT_BUFFSIZE 1024

class Update {
public:
	Update(std::string& device_name, std::string& device_type, std::string& device_id);
	virtual ~Update();
	void task();
private:
	bool update(const char* url);

	int save_autoupdate(const char *filename, uint8_t should_autoupdate);
	int restore_autoupdate(const char *filename, uint8_t *should_autoupdate);
	
    uint8_t should_autoupdate = 1;

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
