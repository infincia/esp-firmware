#pragma once

#include "pch.hpp"

#if defined(CONFIG_FIRMWARE_USE_OTA)

#ifndef UPDATER_H_
#define UPDATER_H_

#include "updater.hpp"


class Updater {
public:
	Updater(const char* update_manifest_url);
	virtual ~Updater();
	void task();
    void start(std::string& device_name);
private:
	void update();

	TaskHandle_t update_task_handle;

	std::string device_name;

    const char* update_url;
};

#endif /* UPDATER_H_ */

#endif /* USE_OTA */
