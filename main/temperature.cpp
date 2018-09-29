#include "pch.hpp"


#if defined(CONFIG_FIRMWARE_USE_TEMPERATURE_SI7021) || defined(CONFIG_FIRMWARE_USE_TEMPERATURE_DHT11)

#include "temperature.hpp"
#include <JSON.h>

extern const char letsencrypt_chain_pem_start[] asm("_binary_letsencrypt_chain_pem_start");
extern const char letsencrypt_chain_pem_end[] asm("_binary_letsencrypt_chain_pem_end");



extern "C" {
#if defined(CONFIG_FIRMWARE_USE_TEMPERATURE_SI7021)
#include "si7021.h"
#elif  defined(CONFIG_FIRMWARE_USE_TEMPERATURE_DHT11)
#include "dht_espidf.h"
#endif
}

static const char *TAG = "[Temperature]";


/**
 *
 * @brief I2C port configuration
 */
#if defined(CONFIG_FIRMWARE_USE_TEMPERATURE_SI7021)
#define SI7021_I2C_MASTER_SCL_IO 22 /*!< gpio number for I2C master clock */
#define SI7021_I2C_MASTER_SDA_IO 21 /*!< gpio number for I2C master data  */
#define SI7021_I2C_MASTER_NUM I2C_NUM_0 /*!< I2C port number for master dev */
#define SI7021_I2C_MASTER_TX_BUF_DISABLE 0
#define SI7021_I2C_MASTER_RX_BUF_DISABLE 0
#define SI7021_I2C_MASTER_FREQ_HZ 100000
#elif defined(CONFIG_FIRMWARE_USE_TEMPERATURE_DHT11)
#define DHT_IO 27
#endif

/**
 *
 * @brief Temperature task wrapper
 */

static void task_wrapper(void *param) {
	auto *instance = static_cast<Temperature *>(param);
    instance->task();
}


Temperature::Temperature(const char* endpoint_url): 
_endpoint_url(endpoint_url) {
}


Temperature::~Temperature() = default;


void Temperature::start(std::string& device_name) {
    this->device_name = device_name;

    #if defined(CONFIG_FIRMWARE_USE_TEMPERATURE_SI7021)
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
    #endif
    xTaskCreate(&task_wrapper, "temperature_task", 4096, this, (tskIDLE_PRIORITY + 10),
        &this->temperature_task_handle);
}


/**
 * @brief Update sensor values
 */

bool Temperature::update() {
    bool success = false;

    esp_err_t ret;

#if defined(CONFIG_FIRMWARE_USE_TEMPERATURE_SI7021)
    struct si7021_reading si7021_data{};

    ret = readSensors(this->port, &si7021_data);

    if (ret == ESP_OK) {
        success = true;
        this->current_temperature = (si7021_data.temperature * 1.8f) + 32.0f;
        this->current_humidity = si7021_data.humidity;
        ESP_LOGD(TAG, "si7021 sensor reading: %f° / %f", this->current_temperature, this->current_humidity);
    }    

#elif defined(CONFIG_FIRMWARE_USE_TEMPERATURE_DHT11)
    dht_result_t res;
    struct dht_reading dht_data{};
    res = read_dht_sensor_data((gpio_num_t)DHT_IO, DHT11, &dht_data);

    if (res == DHT_OK) {
        success = true;
        this->current_temperature = (dht_data.temperature * 1.8f) + 32.0f;
        this->current_humidity = dht_data.humidity;
        ESP_LOGD(TAG, "DHT sensor reading: %f° / %f", this->current_temperature, this->current_humidity);
    }
#endif

    if (success) {
    #if defined(CONFIG_FIRMWARE_USE_WEB)
        try {
            this->send_http();
        } catch (std::exception &ex) {
            ESP_LOGE(TAG, "%s", ex.what());
        }

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
        vTaskDelay(15000 / portTICK_RATE_MS);
    }
}

/**
 *
 * @brief HTTP endpoint support
 */

void Temperature::send_http() {
    long timeout = 5000;
    char user_agent[32];
    snprintf(user_agent, sizeof(user_agent), "%s/%s", "firmware", FIRMWARE_VERSION);

    HTTPSClient http_client(user_agent, letsencrypt_chain_pem_start, timeout);

    int res;
    try {
        memset(this->_text, 0, TEXT_BUFFSIZE);

        http_client.set_read_cb([&] (const char* buf, int length) {
            memcpy(_text, buf, length);
        });

            
        auto json = JSON::createObject();

        json.setDouble("temperature", this->current_temperature);
        json.setDouble("humidity", this->current_humidity);
        json.setString("device_name", this->device_name);

        auto post_body = json.toStringUnformatted();
        const char* _post_body = post_body.c_str();

        ESP_LOGD(TAG, "http request post body: %s", _post_body);

        res = http_client.post(_endpoint_url, _post_body);

        JSON::deleteObject(json);

        if (res >= 500) {
            throw std::runtime_error("server error");
        } else if (res == 404) {
            throw std::runtime_error("no endpoint found at current url");
        } else if (res == 200) {
            ESP_LOGI(TAG, "posted temperature");
        } else {
            ESP_LOGE(TAG, "unknown request error: %d", res);
            throw std::runtime_error("HTTP failed");
        }
        
        ESP_LOGD(TAG, "http request success: %d", res);

        std::string body(_text);

        auto m = JSON::parseObject(body);

        bool success_s = m.getBoolean("success");

        JSON::deleteObject(m);

        if (!success_s) {
            ESP_LOGE(TAG, "success key missing in response");
            throw std::runtime_error("success key missing in response");
        }
    } catch (std::exception &ex) {
        ESP_LOGE(TAG, "http request failed: %s", ex.what());
        throw std::runtime_error("http request failed");
    }
}
#endif
