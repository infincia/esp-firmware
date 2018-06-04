#pragma once

#include "pch.hpp"

#if defined(CONFIG_FIRMWARE_USE_AMP)

#ifndef AMP_H_
#define AMP_H_

class Amp {
public:
	Amp();
	virtual ~Amp();
	void task();

private:
    TaskHandle_t amp_task_handle;

	uint8_t current_volume;
	bool use_spread_spectrum;

	bool increase_volume();
	bool decrease_volume();
	bool set_volume(uint8_t volume);

	int save_volume(const char *filename, uint8_t volume_level);
	int restore_volume(const char *filename, uint8_t *volume_level);
};

#endif /* AMP_H_ */

#endif
