#pragma once

#include "pch.hpp"

#if defined(USE_AMP)

#ifndef OLED_H_
#define OLED_H_

extern "C" {
#include "ssd1306.h"
}

class OLED {
public:
	OLED();
	virtual ~OLED();
	void task();
private:
    void set_text(const char *text);

    SSD1306_Device I2CDisplay;

	TaskHandle_t oled_task_handle;
};

#endif /* OLED_H_ */

#endif
