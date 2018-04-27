#pragma once

#include "pch.hpp"

#if defined(USE_CONSOLE)

#ifndef CONSOLE_H_
#define CONSOLE_H_

class Console {
public:
	Console();
	virtual ~Console();
	void task();
private:
	TaskHandle_t console_task_handle;
};

#endif /* CONSOLE_H_ */

#endif
