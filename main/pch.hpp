#pragma once

#include "sdkconfig.h"

/**
 * Standard library
 *
 */

#include <cctype>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <map>
#include <mutex>

//#include <sstream>
//#include <iostream>
//#include <iomanip>

/**
 * FreeRTOS
 *
 */

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"


/**
 * ESP IDF
 *
 */

#include <esp_console.h>
#include <esp_err.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <esp_attr.h>
#include <esp_partition.h>
#include <esp_sleep.h>
#include <esp_spiffs.h>
#include <esp_system.h>
#include <esp_vfs_dev.h>
#include <esp_vfs_fat.h>
#include <esp_wifi.h>
#include <esp_ota_ops.h>
#include <mdns.h>

#include <nvs.h>
#include <nvs_flash.h>
#include <tcpip_adapter.h>

/**
 * Drivers
 *
 */

#include "driver/i2c.h"
#include "driver/periph_ctrl.h"
#include "driver/rmt.h"
#include "driver/rtc_io.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "soc/rmt_reg.h"
#include "soc/rtc.h"
#include "soc/rtc_cntl_reg.h"


#include "constants.h"

/**
 * 3rd party libraries
 *
 */

#if defined(CONFIG_FIRMWARE_USE_CONSOLE)
#include <argtable3/argtable3.h>
#include <linenoise/linenoise.h>
#endif

#if defined(CONFIG_FIRMWARE_USE_OTA)
#include <HTTPSClient.hpp>
#include <SHA256.hpp>
#endif


/**
 * Local
 *
 */

#if defined(CONFIG_FIRMWARE_USE_CONSOLE)
#include "cmd_decl.hpp"
#endif
#include "globals.hpp"
#include "ipc_command.h"
#include "util.hpp"
