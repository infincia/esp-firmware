/* Console example — declarations of command registration functions.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once

#include "pch.hpp"

#if defined(CONFIG_FIRMWARE_USE_CONSOLE)

#include "constants.h"

void register_system();
void register_volume();

#endif