#pragma once


#include "pch.hpp"

//unsigned long micros();
unsigned long millis();

double round(double d);

long map(int input, int input_start, int input_end, int output_start, int output_end);

bool volume_up();
bool volume_down();
bool volume_set(uint8_t level);

bool set_kv(const char* key, std::string value);
bool get_kv(const char* key, std::string &value);

bool set_kv(const char* key, int32_t value);
bool get_kv(const char* key, int32_t* value);

