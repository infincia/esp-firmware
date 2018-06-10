#pragma once

#include "pch.hpp"

#ifndef LED_H_
#define LED_H_

class LED {
public:
	LED();
	virtual ~LED();
	void task();
    void start();

private:
    TaskHandle_t led_task_handle;

	bool identify;
    bool error;
    bool ready;

};

#endif /* LED_H_ */
