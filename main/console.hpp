#pragma once

#include "pch.hpp"

#if defined(CONFIG_FIRMWARE_USE_CONSOLE)

#ifndef CONSOLE_H_
#define CONSOLE_H_

class Console {
public:
	Console();
	virtual ~Console();
	void task();
    void start();
private:
	TaskHandle_t console_task_handle;
};

#endif /* CONSOLE_H_ */

#endif
