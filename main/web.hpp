#pragma once


#include "pch.hpp"

#if defined(CONFIG_FIRMWARE_USE_WEB)

#ifndef WEB_H_
#define WEB_H_

#include "HttpServer.h"

class Web {
public:
	Web(uint16_t port, std::string& device_name, std::string& device_type, std::string& device_id);
	virtual ~Web();
	void task();

	std::string device_name;
	std::string device_type;
    std::string device_id;
    
private:
	TaskHandle_t web_task_handle;

    uint16_t port;

#if defined(CONFIG_FIRMWARE_USE_WEBSOCKET)
	void send_volume(int current_volume);
    void send_temperature(float current_temperature, float current_humidity);
#endif

    HttpServer webServer;
};

#endif /* WEB_H_ */

#endif /* USE_WEB */
