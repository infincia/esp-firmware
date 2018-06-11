#include "pch.hpp"



#include "led.hpp"

static const char *TAG = "[LED]";



/**
 *
 * @brief LED task wrapper
 */

static void task_wrapper(void *param) {
    auto *instance = static_cast<LED *>(param);
    instance->task();
}


LED::LED(): identify(false), error(false), ready(false) { }


LED::~LED() = default;


void LED::start() {
    ESP_LOGI(TAG, "start");

    xTaskCreate(&task_wrapper, "led_task", 768, this, (tskIDLE_PRIORITY + 10), &this->led_task_handle);
}


/**
 *
 * @brief Task loop
 */

void LED::task() {
    gpio_num_t error_gpio = static_cast<gpio_num_t>(CONFIG_FIRMWARE_ERROR_GPIO);
    gpio_pad_select_gpio(error_gpio);
    gpio_set_direction(error_gpio, GPIO_MODE_OUTPUT);


    gpio_num_t identify_gpio = static_cast<gpio_num_t>(CONFIG_FIRMWARE_IDENTIFY_GPIO);
    gpio_pad_select_gpio(identify_gpio);
    gpio_set_direction(identify_gpio, GPIO_MODE_OUTPUT);

    gpio_num_t ready_gpio = static_cast<gpio_num_t>(CONFIG_FIRMWARE_READY_GPIO);
    gpio_pad_select_gpio(ready_gpio);
    gpio_set_direction(ready_gpio, GPIO_MODE_OUTPUT);

    while (true) {
        LEDMessage message;
        if (xQueueReceive(ledQueue, &(message), (TickType_t)10)) {
            if (message.messageType == EventIdentifyLED) {
                this->identify = message.state;
            } else if (message.messageType == EventErrorLED) {
                this->error = message.state;
            } else if (message.messageType == EventReadyLED) {
                this->ready = message.state;
            }
        }

        if (identify) {
            gpio_set_level(identify_gpio, LED_OFF);
        } else {
            gpio_set_level(identify_gpio, LED_ON);
        }

        if (error) {
            gpio_set_level(error_gpio, LED_OFF);
        } else {
            gpio_set_level(error_gpio, LED_ON);
        }

        if (ready) {
            gpio_set_level(ready_gpio, LED_OFF);
        } else {
            gpio_set_level(ready_gpio, LED_ON);
        }

        vTaskDelay(50 / portTICK_RATE_MS);
    }
}
