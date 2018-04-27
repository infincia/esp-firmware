#include "../pch.hpp"

#if defined(USE_AWS)

#include "aws.hpp"

#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"
#include "aws_iot_shadow_interface.h"

static const char *TAG = "[AWS]";


#define MAX_LENGTH_OF_UPDATE_JSON_BUFFER 200

extern const uint8_t aws_root_ca_pem_start[] asm("_binary_aws_root_ca_pem_start");
extern const uint8_t aws_root_ca_pem_end[] asm("_binary_aws_root_ca_pem_end");


extern const uint8_t aws_crt_start[] asm("_binary_aws_crt_start");
extern const uint8_t aws_crt_end[] asm("_binary_aws_crt_end");

extern const uint8_t aws_key_start[] asm("_binary_aws_key_start");
extern const uint8_t aws_key_end[] asm("_binary_aws_key_end");

// properly silences warning about string literals, only needed in C++ code
char iot_host[] = AWS_IOT_MQTT_HOST;

/**
 *
 * @brief AWS task wrapper
 */

static void task_wrapper(void *param) {
    auto *instance = static_cast<AWS *>(param);
    instance->task();
}


AWS::AWS(std::string& device_name, std::string& device_type, std::string& device_id):
device_name(device_name),
device_type(device_type),
device_id(device_id) {
    xTaskCreate(&task_wrapper, "aws_task", 16384, this, (tskIDLE_PRIORITY + 10), &this->aws_task_handle);
}


AWS::~AWS() = default;


/**
 *
 * @brief AWS IoT thing shadow
 */

static void s_ShadowUpdateStatusCallback(const char *pThingName, ShadowActions_t action, Shadow_Ack_Status_t status,
                                const char *pReceivedJsonDocument, void *pContextData) {
    AWS *instance = static_cast<AWS *>(pContextData);
    instance->ShadowUpdateStatusCallback(pThingName, action, status, pReceivedJsonDocument, pContextData);                         
}


void AWS::ShadowUpdateStatusCallback(const char *pThingName, 
                                     ShadowActions_t action, 
                                     Shadow_Ack_Status_t status, 
                                     const char *pReceivedJsonDocument, 
                                     void *pContextData) {
    IOT_UNUSED(pThingName);
    IOT_UNUSED(action);
    IOT_UNUSED(pReceivedJsonDocument);
    IOT_UNUSED(pContextData);

    this->shadowUpdateInProgress = false;

    if(SHADOW_ACK_TIMEOUT == status) {
        ESP_LOGE(TAG, "update timed out");
    } else if(SHADOW_ACK_REJECTED == status) {
        ESP_LOGE(TAG, "update rejected");
    }
}


/**
 *
 * @brief Task loop
 */

void AWS::task() {    
    /* Wait for wifi to be available */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

    /* Wait for device provisioning */
    xEventGroupWaitBits(provisioning_event_group, PROVISIONED_BIT, false, true, portMAX_DELAY);


    IoT_Error_t rc = FAILURE;
    char JsonDocumentBuffer[MAX_LENGTH_OF_UPDATE_JSON_BUFFER];
    size_t sizeOfJsonDocumentBuffer = sizeof(JsonDocumentBuffer) / sizeof(JsonDocumentBuffer[0]);

    jsonStruct_t temperatureHandler;
    temperatureHandler.cb = nullptr;
    temperatureHandler.pKey = "temperature";
    temperatureHandler.pData = &this->temperature;
    temperatureHandler.type = SHADOW_JSON_FLOAT;

    jsonStruct_t humidityHandler;
    humidityHandler.cb = nullptr;
    humidityHandler.pKey = "humidity";
    humidityHandler.pData = &this->humidity;
    humidityHandler.type = SHADOW_JSON_FLOAT;


    jsonStruct_t heapHandler;
    heapHandler.cb = nullptr;
    heapHandler.pKey = "heap";
    heapHandler.pData = &this->heap;
    heapHandler.type = SHADOW_JSON_UINT32;


    AWS_IoT_Client mqttClient;
    ShadowInitParameters_t sp = ShadowInitParametersDefault;

    sp.pHost = iot_host;

    sp.port = AWS_IOT_MQTT_PORT;

    sp.pClientCRT = (const char *)aws_crt_start;
    sp.pClientKey = (const char *)aws_key_start;    


    sp.pRootCA = (const char *)aws_root_ca_pem_start;

    sp.enableAutoReconnect = true;
    sp.disconnectHandler = nullptr;


    rc = aws_iot_shadow_init(&mqttClient, &sp);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "aws_iot_shadow_init returned error %d, aborting...", rc);
        return;
    }

    ShadowConnectParameters_t scp = ShadowConnectParametersDefault;
    scp.pMyThingName = this->device_name.c_str();
    scp.pMqttClientId = this->device_name.c_str();
    scp.mqttClientIdLen = (uint16_t) strlen(this->device_name.c_str());


    rc = aws_iot_shadow_connect(&mqttClient, &scp);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "aws_iot_shadow_connect returned error %d, aborting...", rc);
        return;
    }


    rc = aws_iot_shadow_set_autoreconnect_status(&mqttClient, true);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "Unable to set Auto Reconnect to true - %d, aborting...", rc);
        return;
    }

    long last = millis();

    int interval = 15000;


    while (true) {
        rc = aws_iot_shadow_yield(&mqttClient, 200);

        SensorMessage message;
        if (xQueueReceive(awsQueue, &(message), (TickType_t)10)) {
            if (message.messageType == EventTemperatureSensorValue) {
                this->temperature = message.temperature;
                this->humidity = message.humidity;
            } else {
                ESP_LOGW(TAG, "unknown message type: %d", message.messageType);
            }
        }

        if(NETWORK_ATTEMPTING_RECONNECT == rc || NETWORK_RECONNECTED == rc || SUCCESS == rc) {
            rc = aws_iot_shadow_yield(&mqttClient, 1000);
            if(NETWORK_ATTEMPTING_RECONNECT != rc && !this->shadowUpdateInProgress) {
                if (this->temperature == 0.0 || this->humidity == 0.0) {
                    continue;
                }
                
                /**
                  * Only update shadow every 60 sec
                  */

                long now = millis();

                if ((now - last) < interval) {
                    continue;
                }

                last = millis();

                size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
                size_t min_free_heap = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);

                ESP_LOGD("[Heap]", "free: %d, min: %d", free_heap, min_free_heap);

                this->heap = (uint32_t)free_heap;

                rc = aws_iot_shadow_init_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
                if(SUCCESS == rc) {
                    rc = aws_iot_shadow_add_reported(JsonDocumentBuffer, 
                                                    sizeOfJsonDocumentBuffer, 
                                                    3, 
                                                    &temperatureHandler,
                                                    &humidityHandler,
                                                    &heapHandler);
                    if(SUCCESS == rc) {
                        rc = aws_iot_finalize_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
                        if(SUCCESS == rc) {
                            rc = aws_iot_shadow_update(&mqttClient, 
                                                       this->device_name.c_str(), 
                                                       JsonDocumentBuffer,
                                                       s_ShadowUpdateStatusCallback, 
                                                       (void*)this, 
                                                       4,
                                                       true);

                            this->shadowUpdateInProgress = true;
                        } else {
                            ESP_LOGW(TAG, "finalizing json document failed");
                        }
                    } else {
                        ESP_LOGW(TAG, "adding reporting handlers failed");
                    }
                } else {
                    ESP_LOGW(TAG, "initializing json document failed");
                }
            }
        }
    }
}

#endif
