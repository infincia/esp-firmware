#include "pch.hpp"


#if defined(CONFIG_FIRMWARE_USE_AMP)


#include "oled.hpp"

extern "C" {
#include "ssd1306_default_if.h"
#include "ssd1306_draw.h"
#include "ssd1306_font.h"
}

#include "font.h"

static const char *TAG = "[OLED]";


/**
 * @brief OLED constants
 */

static const int I2CDisplayAddress = 0x3C;
static const int I2CDisplayWidth = 128;
static const int I2CDisplayHeight = 64;
static const int I2CResetPin = -1;


/**
 * @brief Task wrapper
 */

static void task_wrapper(void *param) {
    auto *instance = static_cast<OLED *>(param);
    instance->task();
}


/**
 * @brief OLED
 *
 */

OLED::OLED() { }


OLED::~OLED() = default;


void OLED::start() {
    if (!SSD1306_I2CMasterInitDefault()) {
        ESP_LOGE(TAG, "I2C init failed");
        abort();
    }

    if(!SSD1306_I2CMasterAttachDisplayDefault(&this->I2CDisplay,
                                              I2CDisplayWidth,
                                              I2CDisplayHeight,
                                              I2CDisplayAddress,
                                              I2CResetPin)) {
        ESP_LOGE(TAG, "Display attach failed");
        abort();
    }

    SSD1306_SetHFlip(&this->I2CDisplay, true);
    SSD1306_SetVFlip(&this->I2CDisplay, true);
    SSD1306_SetContrast(&this->I2CDisplay, 30);

    SSD1306_Clear(&this->I2CDisplay, SSD_COLOR_BLACK);
    SSD1306_SetFont(&this->I2CDisplay, &Font_droid_sans_mono_13x24);
    SSD1306_FontDrawAnchoredString(&this->I2CDisplay, TextAnchor_NorthWest, "Amp", SSD_COLOR_WHITE);
    SSD1306_FontDrawAnchoredString(&this->I2CDisplay, TextAnchor_SouthWest, VERSION, SSD_COLOR_WHITE);

    SSD1306_Update(&this->I2CDisplay);

    SSD1306_SetFont(&this->I2CDisplay, &Font_Tarable7Seg_32x64);

    xTaskCreate(&task_wrapper, "oled_task", 1024, this, (tskIDLE_PRIORITY + 10), &this->oled_task_handle);
}


/**
 *
 * @brief Text handling
 */

void OLED::set_text(const char *text) {
    SSD1306_Clear(&this->I2CDisplay, SSD_COLOR_BLACK);
    SSD1306_FontDrawAnchoredString(&this->I2CDisplay, TextAnchor_Center, text, SSD_COLOR_WHITE);
    SSD1306_Update(&this->I2CDisplay);
}


/**
 *
 * @brief Task loop
 */

void OLED::task() {
    vTaskDelay(15000 / portTICK_RATE_MS);

    while (true) {
        DisplayControlMessage message;
        if (xQueueReceive(displayQueue, &(message), (TickType_t)10)) {
            if (message.messageType == ControlMessageTypeDisplayText) {
                set_text(message.text);
            }
        }
        vTaskDelay(10 / portTICK_RATE_MS);
    }
}

#endif
