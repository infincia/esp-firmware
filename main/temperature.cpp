#include "pch.hpp"


#if defined(CONFIG_FIRMWARE_USE_TEMPERATURE)

#include "temperature.hpp"


extern "C" {
#include "si7021.h"
}

static const char *TAG = "[Temperature]";


/**
 *
 * @brief I2C port configuration
 */
#define SI7021_I2C_MASTER_SCL_IO 22 /*!< gpio number for I2C master clock */
#define SI7021_I2C_MASTER_SDA_IO 21 /*!< gpio number for I2C master data  */
#define SI7021_I2C_MASTER_NUM I2C_NUM_0 /*!< I2C port number for master dev */
#define SI7021_I2C_MASTER_TX_BUF_DISABLE 0
#define SI7021_I2C_MASTER_RX_BUF_DISABLE 0
#define SI7021_I2C_MASTER_FREQ_HZ 100000


/**
 *
 * @brief Temperature task wrapper
 */

static void task_wrapper(void *param) {
	auto *instance = static_cast<Temperature *>(param);
    instance->task();
}


Temperature::Temperature() { }


Temperature::~Temperature() = default;


void Temperature::start() {
    this->port = SI7021_I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = (gpio_num_t)SI7021_I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = (gpio_num_t)SI7021_I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = SI7021_I2C_MASTER_FREQ_HZ;
    i2c_param_config(this->port, &conf);
    i2c_driver_install(this->port, conf.mode, SI7021_I2C_MASTER_RX_BUF_DISABLE,
        SI7021_I2C_MASTER_TX_BUF_DISABLE, 0);

    xTaskCreate(&task_wrapper, "temperature_task", 3072, this, (tskIDLE_PRIORITY + 10),
        &this->temperature_task_handle);
}


/**
 * @brief Update sensor values
 */

bool Temperature::update() {
    struct si7021_reading si7021_data{};

    bool success = false;

    esp_err_t ret;

    ret = readSensors(this->port, &si7021_data);

    if (ret == ESP_OK) {
        success = true;
        this->current_temperature = (si7021_data.temperature * 1.8f) + 32.0f;
        this->current_humidity = si7021_data.humidity;
        ESP_LOGD(TAG, "si7021 sensor reading: %f° / %f", this->current_temperature, this->current_humidity);
    }


    if (success) {
    #if defined(CONFIG_FIRMWARE_USE_WEB)
        {
            IPCMessage message;
            message.messageType = EventTemperatureSensorValue;
            message.temperature = this->current_temperature;
            message.humidity = this->current_humidity;

            ESP_LOGD(TAG, "sending temperature reading to web task");

            if (!xQueueOverwrite(webQueue, (void *)&message)) {
                ESP_LOGV(TAG, "Sending web temperature event failed");
            }
        }
    #endif

    #if defined(CONFIG_FIRMWARE_USE_AWS) || defined(CONFIG_FIRMWARE_USE_HOMEKIT)
        {
            IPCMessage message;
            message.messageType = EventTemperatureSensorValue;
            message.temperature = this->current_temperature;
            message.humidity = this->current_humidity;

    #if defined(CONFIG_FIRMWARE_USE_AWS)
            ESP_LOGD(TAG, "sending temperature reading to aws task");

            if (!xQueueOverwrite(awsQueue, (void *)&message)) {
                ESP_LOGV(TAG, "Sending aws sensor event failed");
            }
    #endif

    #if defined(CONFIG_FIRMWARE_USE_HOMEKIT)
            ESP_LOGD(TAG, "sending temperature reading to homekit task");

            if (!xQueueOverwrite(homekitQueue, (void *)&message)) {
                ESP_LOGV(TAG, "Sending homekit sensor event failed");
            }
    #endif
        }
    #endif

    }

    return success;
}


/**
 *
 * @brief Task loop
 */

void Temperature::task() {
    ESP_LOGI(TAG, "running");
    
    while (true) {
        this->update();
        vTaskDelay(2000 / portTICK_RATE_MS);
    }
}

#endif
