#pragma once

#include "pch.hpp"

#if defined(CONFIG_FIRMWARE_USE_TEMPERATURE)

#ifndef TEMPERATURE_H_
#define TEMPERATURE_H_

class Temperature {
public:
	Temperature(std::string& device_name, std::string& device_type, std::string& device_id);
	virtual ~Temperature();
	void task();

private:
    TaskHandle_t temperature_task_handle;

	std::string device_name;
	std::string device_type;
    std::string device_id;
    
	i2c_port_t port;

	float current_temperature = 0.0f;
	float current_humidity = 0.0f;

	bool update();
};

#endif /* TEMPERATURE_H_ */

#endif
