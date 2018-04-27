#pragma once

#include "pch.hpp"

#if defined(USE_AMP)


#ifndef IR_H_
#define IR_H_

class IR {
public:
	IR();
	virtual ~IR();
	void task();
private:
	TaskHandle_t ir_task_handle;
};

#endif /* IR_H_ */

#endif
