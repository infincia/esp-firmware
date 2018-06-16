#pragma once

#include "../pch.hpp"

#if defined(CONFIG_FIRMWARE_USE_HOMEKIT)

#ifndef HOMEKIT_H_
#define HOMEKIT_H_

class Homekit {
public:
	Homekit();
	virtual ~Homekit();
	void task();
    void start(std::string& device_name, std::string& device_type, std::string& device_id);


	std::string device_name;
	std::string device_type;
 	std::string device_id;

private:
    TaskHandle_t homekit_task_handle;


	float temperature = 0.0;
	float humidity = 0.0;
    uint32_t heap = 0;
	uint8_t volume = 0;

    std::string pin;

};

#endif /* HOMEKIT_H_ */

#endif /* USE_HOMEKIT */
