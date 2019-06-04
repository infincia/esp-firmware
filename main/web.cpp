#include "pch.hpp"

#if defined(CONFIG_FIRMWARE_USE_WEB)

#if defined(CONFIG_FIRMWARE_USE_WEBSOCKET)
#include <algorithm>
#include <iostream>
#include <map>
#include <mutex>
#endif

#include <JSON.h>



#if defined(CONFIG_FIRMWARE_USE_WEBSOCKET)
#include "WebSocket.h"
#include "websocket.hpp"
#endif

#include "web.hpp"


static const char *TAG = "[Web]";

#if defined(CONFIG_FIRMWARE_USE_MDNS)
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
#endif

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

static esp_err_t handle_get_index(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Content-Type", "text/html");
    httpd_resp_send(req, index_page, strlen(index_page));
    return ESP_OK;
}


static esp_err_t handle_get_status(httpd_req_t *req) {
    Web *instance = static_cast<Web *>(req->user_ctx);

    httpd_resp_set_hdr(req, "Content-Type", "application/json");

    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t min_free_heap = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);

    auto json = JSON::createObject();

    json.setInt("f", free_heap);
    json.setInt("m", min_free_heap);
    json.setString("v", FIRMWARE_VERSION);
    json.setInt("vol", instance->volume);
    json.setDouble("t", instance->temperature); 
    json.setDouble("h", instance->humidity); 
    json.setBoolean("hes", instance->heater_state); 
    json.setInt("hel", instance->heater_level); 

    std::string s = json.toStringUnformatted();

    httpd_resp_send(req, s.c_str(), strlen(s.c_str()));


    JSON::deleteObject(json);

    return ESP_OK;
}


static esp_err_t handle_get_provision(httpd_req_t *req) {
    Web *instance = static_cast<Web *>(req->user_ctx);
    httpd_resp_set_hdr(req, "Content-Type", "application/json");


    auto json_response = JSON::createObject();

    json_response.setString("n", instance->device_name); 
    json_response.setString("t", instance->device_type); 

    #if defined(CONFIG_FIRMWARE_USE_HOMEKIT)
    std::string pin;
    if (get_kv(HOMEKIT_PIN_KEY, pin)) {
        json_response.setString("pin", pin); 
    } else {
        json_response.setString("pin", "N/A"); 
    }
    #endif

    std::string s = json_response.toStringUnformatted();

    httpd_resp_send(req, s.c_str(), strlen(s.c_str()));

    JSON::deleteObject(json_response);

    return ESP_OK;

}

static esp_err_t handle_post_heater(httpd_req_t *req) {
    Web *instance = static_cast<Web *>(req->user_ctx);

    httpd_resp_set_hdr(req, "Content-Type", "application/json");

    /* Truncate if content length larger than the buffer */
    size_t recv_size = MIN(req->content_len, sizeof(instance->content));

    int ret = httpd_req_recv(req, instance->content, recv_size);
    if (ret <= 0) {  /* 0 return value indicates connection closed */
        /* Check if timeout occurred */
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            /* In case of timeout one can choose to retry calling
             * httpd_req_recv(), but to keep it simple, here we
             * respond with an HTTP 408 (Request Timeout) error */
            httpd_resp_send_408(req);
        }
        /* In case of error, returning ESP_FAIL will
         * ensure that the underlying socket is closed */
        return ESP_FAIL;
    }

    std::string body(reinterpret_cast<char*>(instance->content), recv_size);


    auto m = JSON::parseObject(body);

	std::string temp1;
    bool heater_state = m.getBoolean("hes");

    if (heater_state) {
        temp1 = "1";
    } else {
        temp1 = "0";
    }
    
    static char temp2[100];
    auto heater_level = m.getInt("hel");
    sprintf(temp2, "%d", heater_level);

    JSON::deleteObject(m);

    auto json_response = JSON::createObject();

    IPCMessage message;
    message.messageType = EventTemperatureHeaterControl;
    message.heater_state = heater_state;
    message.heater_level = heater_level;

    if (!xQueueOverwrite(heaterQueue, (void *)&message)) {
        ESP_LOGE(TAG, "Setting heater state failed");
        json_response.setBoolean("success", false); 
    } else {
        json_response.setBoolean("success", true); 
    }

    std::string s = json_response.toStringUnformatted();
    httpd_resp_send(req, s.c_str(), strlen(s.c_str()));
    JSON::deleteObject(json_response);
    return ESP_OK;
}

static esp_err_t handle_post_provision(httpd_req_t *req) {
    Web *instance = static_cast<Web *>(req->user_ctx);

    httpd_resp_set_hdr(req, "Content-Type", "application/json");

    /* Truncate if content length larger than the buffer */
    size_t recv_size = MIN(req->content_len, sizeof(instance->content));

    int ret = httpd_req_recv(req, instance->content, recv_size);
    if (ret <= 0) {  /* 0 return value indicates connection closed */
        /* Check if timeout occurred */
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            /* In case of timeout one can choose to retry calling
             * httpd_req_recv(), but to keep it simple, here we
             * respond with an HTTP 408 (Request Timeout) error */
            httpd_resp_send_408(req);
        }
        /* In case of error, returning ESP_FAIL will
         * ensure that the underlying socket is closed */
        return ESP_FAIL;
    }

    std::string body(reinterpret_cast<char*>(instance->content), recv_size);


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

    std::string s = json_response.toStringUnformatted();

    httpd_resp_send(req, s.c_str(), strlen(s.c_str()));


    JSON::deleteObject(json_response);

    // this gives the ESP time to send the response before restarting, otherwise
    // it will immediately restart before the browser gets a response, making it
    // look like the HTTP request failed even though it succeeded
    if (restart) {
        vTaskDelay(2000 / portTICK_RATE_MS);
        esp_restart();
    }

    return ESP_OK;
}


static esp_err_t handle_post_identify(httpd_req_t *req) {
    Web *instance = static_cast<Web *>(req->user_ctx);

    httpd_resp_set_hdr(req, "Content-Type", "application/json");


    /* Truncate if content length larger than the buffer */
    size_t recv_size = MIN(req->content_len, sizeof(instance->content));

    int ret = httpd_req_recv(req, instance->content, recv_size);
    if (ret <= 0) {  /* 0 return value indicates connection closed */
        /* Check if timeout occurred */
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            /* In case of timeout one can choose to retry calling
             * httpd_req_recv(), but to keep it simple, here we
             * respond with an HTTP 408 (Request Timeout) error */
            httpd_resp_send_408(req);
        }
        /* In case of error, returning ESP_FAIL will
         * ensure that the underlying socket is closed */
        return ESP_FAIL;
    }

    std::string body(reinterpret_cast<char*>(instance->content), recv_size);

    auto m = JSON::parseObject(body);

    auto identify = m.getBoolean("identify");

    JSON::deleteObject(m);

    auto json_response = JSON::createObject();

    IPCMessage message;
    message.messageType = EventIdentifyLED;
    message.led_state = identify;

    if (!xQueueOverwrite(ledQueue, (void *)&message)) {
        ESP_LOGE(TAG, "Setting LED identify failed");
        json_response.setBoolean("success", false); 
    } else {
        json_response.setBoolean("success", true); 
    }
    
    std::string s = json_response.toStringUnformatted();

    httpd_resp_send(req, s.c_str(), strlen(s.c_str()));

    JSON::deleteObject(json_response);

    return ESP_OK;
}

#if defined(CONFIG_FIRMWARE_USE_AMP)

static esp_err_t handle_post_volume_set(httpd_req_t *req) {
    Web *instance = static_cast<Web *>(req->user_ctx);

    httpd_resp_set_hdr(req, "Content-Type", "application/json");

    /* Truncate if content length larger than the buffer */
    size_t recv_size = MIN(req->content_len, sizeof(instance->content));

    int ret = httpd_req_recv(req, instance->content, recv_size);
    if (ret <= 0) {  /* 0 return value indicates connection closed */
        /* Check if timeout occurred */
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            /* In case of timeout one can choose to retry calling
             * httpd_req_recv(), but to keep it simple, here we
             * respond with an HTTP 408 (Request Timeout) error */
            httpd_resp_send_408(req);
        }
        /* In case of error, returning ESP_FAIL will
         * ensure that the underlying socket is closed */
        return ESP_FAIL;
    }

    std::string body(reinterpret_cast<char*>(instance->content), recv_size);

    auto m = JSON::parseObject(body);

    auto vol = m.getInt("vol");

    JSON::deleteObject(m);

    auto json_response = JSON::createObject();

    if (!volume_set(vol)) {
        ESP_LOGE(TAG, "Setting volume failed");
        json_response.setBoolean("success", false); 
    } else {
        json_response.setBoolean("success", true); 
    }

    std::string s = json_response.toStringUnformatted();

    httpd_resp_send(req, s.c_str(), strlen(s.c_str()));


    JSON::deleteObject(json_response);

    return ESP_OK;
}

static esp_err_t handle_post_volume_up(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Content-Type", "application/json");

    auto json_response = JSON::createObject();

    if (!volume_up()) {
        ESP_LOGE(TAG, "Setting volume up failed");
        json_response.setBoolean("success", false); 
    } else {
        json_response.setBoolean("success", true); 
    }

    std::string s = json_response.toStringUnformatted();

    httpd_resp_send(req, s.c_str(), strlen(s.c_str()));

    JSON::deleteObject(json_response);

    return ESP_OK;
}

static esp_err_t handle_post_volume_down(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Content-Type", "application/json");

    auto json_response = JSON::createObject();

    if (!volume_down()) {
        ESP_LOGE(TAG, "Setting volume down failed");
        json_response.setBoolean("success", false); 
    } else {
        json_response.setBoolean("success", true); 
    }

    std::string s = json_response.toStringUnformatted();

    httpd_resp_send(req, s.c_str(), strlen(s.c_str()));

    JSON::deleteObject(json_response);

    return ESP_OK;
}

#endif

static esp_err_t handle_post_restart(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Content-Type", "application/json");

    auto json_response = JSON::createObject();

    json_response.setBoolean("success", true); 

    std::string s = json_response.toStringUnformatted();

    httpd_resp_send(req, s.c_str(), strlen(s.c_str()));

    JSON::deleteObject(json_response);

    vTaskDelay(2000 / portTICK_RATE_MS);
    esp_restart();

    return ESP_OK;
}

#if defined(CONFIG_FIRMWARE_USE_WEBSOCKET)
static esp_err_t handle_get_websocket(httpd_req_t *req) {
    auto ws = request->getWebSocket();
    if (ws != nullptr) {
        Conn *c = new Conn(ws);
        ws->setHandler(c);
        std::lock_guard<std::mutex> guard(conns_mutex);
        conns.push_back(c);
        int len = conns.size();

        ESP_LOGD(TAG, "Added connection to list, now have %d connections", len);
    }

    return ESP_OK;
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
    if (this->server) {
        httpd_stop(this->server);
    }
}

void Web::start(std::string& device_name, std::string& device_type) {
    ESP_LOGD(TAG, "start");

    this->device_name = device_name;
    this->device_type = device_type;

    xTaskCreate(&task_wrapper, "web_task", 3072, this, (tskIDLE_PRIORITY + 10), &this->web_task_handle);
}

void Web::configure() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&this->server, &config) == ESP_OK) {
        httpd_uri_t uri_get_index = {
            .uri      = "/",
            .method   = HTTP_GET,
            .handler  = handle_get_index,
            .user_ctx = this
        };
        httpd_register_uri_handler(this->server, &uri_get_index);

 
        #if defined(CONFIG_FIRMWARE_USE_WEBSOCKET)
        httpd_uri_t uri_ws_get = {
            .uri      = "/ws",
            .method   = HTTP_GET,
            .handler  = handle_websocket,
            .user_ctx = this
        };
        httpd_register_uri_handler(this->server, &uri_ws_get);
        #endif


        httpd_uri_t uri_get_status = {
            .uri      = "/status",
            .method   = HTTP_GET,
            .handler  = handle_get_status,
            .user_ctx = this
        };
        httpd_register_uri_handler(this->server, &uri_get_status);

        httpd_uri_t uri_post_heater = {
            .uri      = "/heater",
            .method   = HTTP_POST,
            .handler  = handle_post_heater,
            .user_ctx = this
        };
        httpd_register_uri_handler(this->server, &uri_post_heater);


        httpd_uri_t uri_post_provision = {
            .uri      = "/provision",
            .method   = HTTP_POST,
            .handler  = handle_post_provision,
            .user_ctx = this
        };
        httpd_register_uri_handler(this->server, &uri_post_provision);


        httpd_uri_t uri_get_provision = {
            .uri      = "/provision",
            .method   = HTTP_GET,
            .handler  = handle_get_provision,
            .user_ctx = this
        };
        httpd_register_uri_handler(this->server, &uri_get_provision);


        httpd_uri_t uri_post_identify = {
            .uri      = "/identify",
            .method   = HTTP_POST,
            .handler  = handle_post_identify,
            .user_ctx = this
        };
        httpd_register_uri_handler(this->server, &uri_post_identify);


        httpd_uri_t uri_post_restart = {
            .uri      = "/restart",
            .method   = HTTP_POST,
            .handler  = handle_post_restart,
            .user_ctx = this
        };
        httpd_register_uri_handler(this->server, &uri_post_restart);


        #if defined(CONFIG_FIRMWARE_USE_AMP)
        httpd_uri_t uri_post_volume_set = {
            .uri      = "/volume/set",
            .method   = HTTP_POST,
            .handler  = handle_post_volume_set,
            .user_ctx = this
        };
        httpd_register_uri_handler(this->server, &uri_post_volume_set);


        httpd_uri_t uri_post_volume_up = {
            .uri      = "/volume/up",
            .method   = HTTP_POST,
            .handler  = handle_post_volume_up,
            .user_ctx = this
        };
        httpd_register_uri_handler(this->server, &uri_post_volume_up);


        httpd_uri_t uri_post_volume_down = {
            .uri      = "/volume/down",
            .method   = HTTP_POST,
            .handler  = handle_post_volume_down,
            .user_ctx = this
        };
        httpd_register_uri_handler(this->server, &uri_post_volume_down);
        #endif
    }
}

void Web::task() {
    ESP_LOGI(TAG, "running");

    /* Wait for WiFI to show as connected */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

    this->configure();

#if defined(CONFIG_FIRMWARE_USE_MDNS)
    initialise_mdns(this->device_name.c_str(), this->port);
#endif

    while (true) {
        IPCMessage message;
        if (xQueueReceive(webQueue, &(message), (TickType_t)10)) {
            ESP_LOGV(TAG, "message received");

            if (message.messageType == ControlMessageTypeVolumeEvent) {
                this->volume = message.volumeLevel;
                #if defined(CONFIG_FIRMWARE_USE_WEBSOCKET)
                this->send_volume(this->volume);
                #endif
            } else if (message.messageType == EventTemperatureSensorValue) {
                this->temperature = message.temperature;
                this->humidity = message.humidity;
                this->heater_state = message.heater_state;
                this->heater_level = message.heater_level;

                #if defined(CONFIG_FIRMWARE_USE_WEBSOCKET)
                this->send_temperature(this->temperature, this->humidity);
                #endif
            }
        }

        vTaskDelay(30 / portTICK_RATE_MS);
    }
}

#endif