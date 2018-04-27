#pragma once

#include "../pch.hpp"

#if defined(USE_HOMEKIT)

#ifndef HOMEKIT_H_
#define HOMEKIT_H_

class Homekit {
public:
	Homekit(std::string& device_name, std::string& device_type, std::string& device_id);
	virtual ~Homekit();
	void task();


	std::string device_name;
	std::string device_type;
 	std::string device_id;

private:
    TaskHandle_t homekit_task_handle;


	float temperature = 0.0;
	float humidity = 0.0;
    uint32_t heap = 0;

};

#endif /* HOMEKIT_H_ */

#endif /* USE_HOMEKIT */
