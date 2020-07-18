#include "pch.hpp"


#if defined(USE_TEMPERATURE)

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


Temperature::Temperature(std::string& device_name, std::string& device_type, std::string& device_id): 
device_name(device_name), 
device_type(device_type),
device_id(device_id) {

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

    xTaskCreate(&task_wrapper, "temperature_task", 4096, this, (tskIDLE_PRIORITY + 10),
        &this->temperature_task_handle);
}


Temperature::~Temperature() = default;


/**
 * @brief Update sensor values
 */

bool Temperature::update() {
    struct si7021_reading sensor_data{};

    esp_err_t ret = readSensors(this->port, &sensor_data);

    if (ret != ESP_OK) {
        // ESP_LOGE(TAG, "sensor reading failed");
        return false;
    }

    this->current_temperature = (sensor_data.temperature * 1.8f) + 32.0f;
    this->current_humidity = sensor_data.humidity;

#if defined(USE_WEB)
    {
        WebControlMessage message;
        message.messageType = ControlMessageTypeTemperatureEvent;
        message.temperature = this->current_temperature;
        message.humidity = this->current_humidity;

        if (!xQueueSend(webQueue, (void *)&message, (TickType_t)0)) {
            ESP_LOGV(TAG, "Sending web temperature event failed");
        }
    }
#endif

#if defined(USE_AWS) || defined(USE_HOMEKIT)
    {
        SensorMessage message;
        message.messageType = EventTemperatureSensorValue;
        message.temperature = this->current_temperature;
        message.humidity = this->current_humidity;

#if defined(USE_AWS)
        if (!xQueueSend(awsQueue, (void *)&message, (TickType_t)0)) {
            ESP_LOGV(TAG, "Sending aws sensor event failed");
        }
#endif

#if defined(USE_HOMEKIT)
        if (!xQueueSend(homekitQueue, (void *)&message, (TickType_t)0)) {
            ESP_LOGV(TAG, "Sending homekit sensor event failed");
        }
#endif
    }
#endif
    return true;
}


/**
 *
 * @brief Task loop
 */

void Temperature::task() {
    while (true) {
        this->update();
        vTaskDelay(2000 / portTICK_RATE_MS);
    }
}

#endif
