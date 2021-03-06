#pragma once

#include "../pch.hpp"

#if defined(CONFIG_FIRMWARE_USE_AWS)

#ifndef AWS_H_
#define AWS_H_

#include "aws_iot_shadow_interface.h"

class AWS {
public:
	AWS();
	virtual ~AWS();
	void task();
    void start(std::string& device_name);

	void ShadowUpdateStatusCallback(const char *pThingName, 
                                    ShadowActions_t action, 
                                    Shadow_Ack_Status_t status, 
                                    const char *pReceivedJsonDocument, 
                                    void *pContextData);

private:
    TaskHandle_t aws_task_handle;

	std::string device_name;
 
 	bool shadowUpdateInProgress = false;

	float temperature = 0.0;
	float humidity = 0.0;
    uint32_t heap = 0;
};

#endif /* AWS_H_ */

#endif /* USE_AWS */
