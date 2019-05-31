#pragma once

#include "pch.hpp"

#if defined(CONFIG_FIRMWARE_USE_TEMPERATURE_SI7021) || defined(CONFIG_FIRMWARE_USE_TEMPERATURE_DHT11)

#ifndef TEMPERATURE_H_
#define TEMPERATURE_H_

#define TEXT_BUFFSIZE 1024


class Temperature {
public:
#if defined(CONFIG_FIRMWARE_TEMPERATURE_HTTP_ENDPOINT)
	Temperature(const char* endpoint_url);
#else
    Temperature();
#endif
	virtual ~Temperature();
	void task();
    void start(std::string& device_name);

private:
    TaskHandle_t temperature_task_handle;

	i2c_port_t port;

	float current_temperature = 0.0f;
	float current_humidity = 0.0f;

    bool heater_state = false;
    uint8_t heater_level = 0;

	std::string device_name;

	bool update();

#if defined(CONFIG_FIRMWARE_TEMPERATURE_HTTP_ENDPOINT)
    void send_http();
    const char* _endpoint_url;
    char _text[TEXT_BUFFSIZE + 1] = {0};
#endif


#if defined(CONFIG_FIRMWARE_TEMPERATURE_UDP_ENDPOINT)
    void udp_endpoint_init(const char *ipaddr, unsigned long port );

    int fd;
    struct sockaddr_in serveraddr;
#endif

    std::string packet;
};

#endif /* TEMPERATURE_H_ */

#endif
