#pragma once


#include "pch.hpp"

//unsigned long micros();
unsigned long millis();

bool volume_up();
bool volume_down();
bool volume_set(uint8_t level);

bool set_kv(const char* key, std::string value);
bool get_kv(const char* key, std::string &value);

bool set_kv(const char* key, int32_t value);
bool get_kv(const char* key, int32_t* value);

const char* error_string(esp_err_t err);
