#include "pch.hpp"

#if defined(CONFIG_FIRMWARE_USE_WEB)

#if defined(CONFIG_FIRMWARE_USE_WEBSOCKET)
#include <algorithm>
#include <iostream>
#include <map>
#include <mutex>
#endif

#include <JSON.h>

#include "HttpRequest.h"
#include "HttpResponse.h"

#if defined(CONFIG_FIRMWARE_USE_WEBSOCKET)
#include "WebSocket.h"
#include "websocket.hpp"
#endif

#include "web.hpp"


static const char *TAG = "[Web]";

static void initialise_mdns(const char* name, uint16_t port) {
    //initialize mDNS
    ESP_ERROR_CHECK( mdns_init() );
    //set mDNS hostname (required if you want to advertise services)
    ESP_ERROR_CHECK( mdns_hostname_set(name) );
    //set default mDNS instance name
    ESP_ERROR_CHECK( mdns_instance_name_set(name) );

    //structure with TXT records
    char path_v[] = "/";
    char path_k[] = "path";
    mdns_txt_item_t serviceTxtData[1] = {
        {path_k, path_v},
    };

    //initialize service
    ESP_ERROR_CHECK( mdns_service_add(name, "_http", "_tcp", port, serviceTxtData, 1) );
}

/**
 *  Websocket connection tracking 
 * 
 * */

#if defined(CONFIG_FIRMWARE_USE_WEBSOCKET)
std::mutex conns_mutex;
std::vector<Conn *> conns;
#endif

const char *index_page =
#include "html/index.html"
;


/**
 *
 * @brief Web task wrapper
 */

static void task_wrapper(void *param) {
    ESP_LOGD(TAG, "task_wrapper(): %p", task_wrapper);

    Web *instance = static_cast<Web *>(param);
    instance->task();
}


/**
 *
 * @brief Web routes
 */

static void handle_index(HttpRequest *request, HttpResponse *response, void* ctx) {
    response->addHeader("Content-Type", "text/html");
    response->setStatus(HttpResponse::HTTP_STATUS_OK, "OK");
    response->sendData(index_page);
    response->close();
}


static void handle_get_status(HttpRequest *request, HttpResponse *response, void* ctx) {
    response->addHeader("Content-Type", "application/json");
    response->setStatus(HttpResponse::HTTP_STATUS_OK, "OK");
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t min_free_heap = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);

    auto json = JSON::createObject();

    json.setInt("f", free_heap);
    json.setInt("m", min_free_heap);

    response->sendData(json.toStringUnformatted());

    response->close();

    JSON::deleteObject(json);
}


static void handle_get_provision(HttpRequest *request, HttpResponse *response, void* ctx) {
    Web *instance = static_cast<Web *>(ctx);

    response->addHeader("Content-Type", "application/json");
    response->setStatus(HttpResponse::HTTP_STATUS_OK, "OK");

    auto json_response = JSON::createObject();

    json_response.setString("n", instance->device_name); 
    json_response.setString("t", instance->device_type); 

    response->sendData(json_response.toStringUnformatted());
    response->close();

    JSON::deleteObject(json_response);

}

static void handle_set_provision(HttpRequest *request, HttpResponse *response, void* ctx) {
    response->addHeader("Content-Type", "application/json");
    response->setStatus(HttpResponse::HTTP_STATUS_OK, "OK");

    std::string body(request->getBody());

    auto m = JSON::parseObject(body);

    auto dname = m.getString("n");
    auto dtype = m.getString("t");

    JSON::deleteObject(m);

    auto json_response = JSON::createObject();

    bool restart = false;

    if (set_kv(DEVICE_PROVISIONING_NAME_KEY, dname.c_str()) && set_kv(DEVICE_PROVISIONING_TYPE_KEY, dtype.c_str())) {
        ESP_LOGI(TAG, "device provisioned, restarting");
        json_response.setBoolean("success", true); 
        restart = true;
    } else {
        ESP_LOGW(TAG, "device provisioning could not be saved");
        json_response.setBoolean("success", false); 
    }

    response->sendData(json_response.toStringUnformatted());
    response->close();

    JSON::deleteObject(json_response);

    // this gives the ESP time to send the response before restarting, otherwise
    // it will immediately restart before the browser gets a response, making it
    // look like the HTTP request failed even though it succeeded
    if (restart) {
        vTaskDelay(2000 / portTICK_RATE_MS);
        esp_restart();
    }
}


static void handle_set_identify(HttpRequest *request, HttpResponse *response, void* ctx) {
    response->addHeader("Content-Type", "application/json");
    response->setStatus(HttpResponse::HTTP_STATUS_OK, "OK");

    std::string body(request->getBody());

    auto m = JSON::parseObject(body);

    auto identify = m.getBoolean("identify");

    JSON::deleteObject(m);

    auto json_response = JSON::createObject();

    LEDMessage message;
    message.messageType = EventIdentifyLED;
    message.state = identify;

    if (!xQueueSend(ledQueue, (void *)&message, (TickType_t)0)) {
        ESP_LOGE(TAG, "Setting LED identify failed");
        json_response.setBoolean("success", false); 
    } else {
        json_response.setBoolean("success", true); 
    }
    
    response->sendData(json_response.toStringUnformatted());
    response->close();

    JSON::deleteObject(json_response);

}

#if defined(CONFIG_FIRMWARE_USE_WEBSOCKET)
static void handle_websocket(HttpRequest *request, HttpResponse *response, void* ctx) {
    auto ws = request->getWebSocket();
    if (ws != nullptr) {
        Conn *c = new Conn(ws);
        ws->setHandler(c);
        std::lock_guard<std::mutex> guard(conns_mutex);
        conns.push_back(c);
        int len = conns.size();

        ESP_LOGD(TAG, "Added connection to list, now have %d connections", len);
    }
}

void Web::send_volume(int current_volume) {
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t min_free_heap = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);

    auto json_response = JSON::createObject();

    json_response.setInt("v", current_volume); 
    json_response.setInt("f", free_heap); 
    json_response.setInt("m", min_free_heap); 

    std::string response_body(json_response.toStringUnformatted());

    std::lock_guard<std::mutex> guard(conns_mutex);

    for (Conn *c : conns) {
        ESP_LOGV(TAG, "Sending json to connected websocket");
        try {
            c->send(response_body);
        } catch (std::exception e) {
            ESP_LOGE(TAG, "Exception occurred during websocket send: %s", e.what());
        }
    }
    JSON::deleteObject(json_response);
}

void Web::send_temperature(float current_temperature, float current_humidity) {
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t min_free_heap = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);

    auto json_response = JSON::createObject();

    json_response.setInt("f", free_heap); 
    json_response.setInt("m", min_free_heap); 
    json_response.setDouble("t", current_temperature); 
    json_response.setDouble("h", current_humidity); 
    
    std::string response_body(json_response.toStringUnformatted());

    std::lock_guard<std::mutex> guard(conns_mutex);

    for (Conn *c : conns) {
        ESP_LOGV(TAG, "Sending json to connected websocket");
        try {
            c->send(response_body);
        } catch (std::exception e) {
            ESP_LOGE(TAG, "Exception occurred during websocket send: %s", e.what());
        }
    }
    JSON::deleteObject(json_response);
}
#endif

/**
 *
 * @brief Web class
 */


Web::Web(uint16_t port): port(port) { }


Web::~Web() {
    this->webServer.stop();
}

void Web::start(std::string& device_name, std::string& device_type, std::string& device_id) {
    ESP_LOGD(TAG, "start");

    this->device_name = device_name;
    this->device_type = device_type;
    this->device_id = device_id;

    xTaskCreate(&task_wrapper, "web_task", 8192, this, (tskIDLE_PRIORITY + 10), &this->web_task_handle);
}


void Web::task() {
    ESP_LOGD(TAG, "task running");

    /* Wait for WiFI to show as connected */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

    this->webServer.setContext(this);

    this->webServer.addPathHandler("GET", "/", handle_index);

#if defined(CONFIG_FIRMWARE_USE_WEBSOCKET)
    this->webServer.addPathHandler("GET", "/ws", handle_websocket);
#endif

    this->webServer.addPathHandler("GET", "/status", handle_get_status);

    this->webServer.addPathHandler("POST", "/provision", handle_set_provision);

    this->webServer.addPathHandler("GET", "/provision", handle_get_provision);

    this->webServer.addPathHandler("POST", "/identify", handle_set_identify);

    this->webServer.start(this->port);

    initialise_mdns(this->device_name.c_str(), this->port);

    while (true) {
        WebControlMessage message;
        if (xQueueReceive(webQueue, &(message), (TickType_t)10)) {
            ESP_LOGV(TAG, "message received");

#if defined(CONFIG_FIRMWARE_USE_WEBSOCKET)
            if (message.messageType == ControlMessageTypeVolumeEvent) {
                auto current_volume = message.volumeLevel;
                this->send_volume(current_volume);
            } else if (message.messageType == ControlMessageTypeTemperatureEvent) {
                auto current_temperature = message.temperature;
                auto current_humidity = message.humidity;
                this->send_temperature(current_temperature, current_humidity);
            } else {
                ESP_LOGW(TAG, "unknown message type received: %d", message.messageType);
            }
#endif
        }

        vTaskDelay(10 / portTICK_RATE_MS);
    }
}

#endif