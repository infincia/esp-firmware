#include "pch.hpp"


#if defined(CONFIG_FIRMWARE_USE_TEMPERATURE_SI7021) || defined(CONFIG_FIRMWARE_USE_TEMPERATURE_DHT11)
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

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

static void temperature_update(void *args);

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
#endif

#if defined(CONFIG_FIRMWARE_USE_TEMPERATURE_DHT11)
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

#if defined(CONFIG_FIRMWARE_TEMPERATURE_HTTP_ENDPOINT)
Temperature::Temperature(const char* endpoint_url): 
_endpoint_url(endpoint_url) {
}
#else
Temperature::Temperature() {}
#endif


Temperature::~Temperature() { 

    esp_timer_stop(this->timer);
    esp_timer_delete(this->timer);
}

#if defined(CONFIG_FIRMWARE_TEMPERATURE_UDP_ENDPOINT)
void Temperature::udp_endpoint_init(const char *ipaddr, unsigned long port ) {
    ESP_LOGI(TAG, "initializing udp endpoint...");
    if( (this->fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
       ESP_LOGE(TAG, "can't open socket");
       return;
    }

    uint32_t ip_addr_bytes;
    inet_aton(ipaddr, &ip_addr_bytes);
    ESP_LOGI(TAG, "sending packets to to 0x%x", ip_addr_bytes);

    memset( &this->serveraddr, 0, sizeof(this->serveraddr) );
    this->serveraddr.sin_family = AF_INET;
    this->serveraddr.sin_port = htons( port );
    this->serveraddr.sin_addr.s_addr = ip_addr_bytes;

    return;
}
#endif

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

    #if defined(CONFIG_FIRMWARE_TEMPERATURE_UDP_ENDPOINT)
    udp_endpoint_init(CONFIG_FIRMWARE_TEMPERATURE_UDP_IP, CONFIG_FIRMWARE_TEMPERATURE_UDP_PORT);
    #endif

    memset( &this->timer_args, 0, sizeof(this->timer_args) );
    this->timer_args.name = "temperature_update";
    this->timer_args.callback = temperature_update;
    this->timer_args.arg = this;
    this->timer_args.dispatch_method = ESP_TIMER_TASK;


    esp_timer_create(&this->timer_args, &this->timer);
    esp_timer_start_periodic(this->timer, 3000000);
    
    xTaskCreate(&task_wrapper, "temperature_task", 8192, this, (tskIDLE_PRIORITY + 10),
        &this->temperature_task_handle);
}


/**
 * @brief Update sensor values
 */

static void temperature_update(void *args) {
    Temperature *instance = static_cast<Temperature *>(args);


    bool success = false;

    esp_err_t ret;

#if defined(CONFIG_FIRMWARE_USE_TEMPERATURE_SI7021)
    struct si7021_reading si7021_data{};

    ret = readSensors(instance->port, &si7021_data);

    if (ret == ESP_OK) {
        success = true;
        instance->current_temperature = (si7021_data.temperature * 1.8f) + 32.0f;
        instance->current_humidity = si7021_data.humidity;
        ESP_LOGD(TAG, "si7021 sensor reading: %f° / %f", instance->current_temperature, instance->current_humidity);
    }    

#elif defined(CONFIG_FIRMWARE_USE_TEMPERATURE_DHT11)
    dht_result_t res;
    struct dht_reading dht_data{};
    res = read_dht_sensor_data((gpio_num_t)DHT_IO, DHT11, &dht_data);

    if (res == DHT_OK) {
        success = true;
        instance->current_temperature = (dht_data.temperature * 1.8f) + 32.0f;
        instance->current_humidity = dht_data.humidity;
        ESP_LOGD(TAG, "DHT sensor reading: %f° / %f", instance->current_temperature, instance->current_humidity);
    }
#endif

    if (success) {    
        
        auto json = JSON::createObject();

        json.setBoolean("heater", instance->heater_state);

        json.setDouble("temperature", instance->current_temperature);
        json.setDouble("humidity", instance->current_humidity);
        json.setString("device_name", instance->device_name);

        instance->packet = json.toStringUnformatted();
        const char* _packet = instance->packet.c_str();

        JSON::deleteObject(json);


#if defined(CONFIG_FIRMWARE_TEMPERATURE_UDP_ENDPOINT)
        ESP_LOGD(TAG, "Sending UDP packet: %s", _packet);
        sendto(instance->fd, _packet, strlen(_packet), 0, (struct sockaddr *)&instance->serveraddr, sizeof(instance->serveraddr));
#endif

#if defined(CONFIG_FIRMWARE_TEMPERATURE_HTTP_ENDPOINT)
        try {
            instance->send_http();
        } catch (std::exception &ex) {
            ESP_LOGE(TAG, "%s", ex.what());
        }
#endif

    #if defined(CONFIG_FIRMWARE_USE_WEB)

        {
            IPCMessage message;
            message.messageType = EventTemperatureSensorValue;
            message.temperature = instance->current_temperature;
            message.humidity = instance->current_humidity;
            message.heater_state = instance->heater_state;
            message.heater_level= instance->heater_level;

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
            message.temperature = instance->current_temperature;
            message.humidity = instance->current_humidity;

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

}


/**
 *
 * @brief Task loop
 */

void Temperature::task() {
    ESP_LOGI(TAG, "running");

    esp_err_t err;
    err = getHeaterState(this->port, &this->heater_state);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "getHeaterState: %s", esp_err_to_name(err));
    } else {
        ESP_LOGW(TAG, "getHeaterState: %d", this->heater_state);

    }
    err = getHeaterLevel(this->port, &this->heater_level);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "getHeaterLevel: %s", esp_err_to_name(err));
    } else {
        ESP_LOGW(TAG, "getHeaterLevel: %d", this->heater_level);
    }
    while (true) {
        IPCMessage message;
        if (xQueueReceive(heaterQueue, &(message), (TickType_t)10)) {
            ESP_LOGV(TAG, "message received");

            if (message.messageType == EventTemperatureHeaterControl) {
                this->heater_state = message.heater_state;
                this->heater_level = message.heater_level;

                err = setHeaterState(this->port, this->heater_level, this->heater_state);
                if (err != ESP_OK)
                    ESP_LOGE(TAG, "setHeaterState: %s", esp_err_to_name(err));
            }
        }
        vTaskDelay(30 / portTICK_RATE_MS);
    }
}

/**
 *
 * @brief HTTP endpoint support
 */
#if defined(CONFIG_FIRMWARE_TEMPERATURE_HTTP_ENDPOINT)

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

        const char* _post_body = this->packet.c_str();

        ESP_LOGD(TAG, "http request post body: %s", _post_body);

        res = http_client.post(_endpoint_url, _post_body);

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

#endif
