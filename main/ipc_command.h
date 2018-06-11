#pragma once

typedef enum message_type {
    ControlMessageTypeVolumeUp,
    ControlMessageTypeVolumeDown,
    ControlMessageTypeVolumeSet,
    ControlMessageTypeDisplayText,
    ControlMessageTypeVolumeEvent,
    EventTemperatureSensorValue,
    EventIdentifyLED,
    EventErrorLED,
    EventReadyLED
} MessageType;


typedef struct volume_message {
    MessageType messageType;
    uint8_t volumeLevel;
} VolumeControlMessage;

typedef struct display_message {
    MessageType messageType;
    char text[3];
} DisplayControlMessage;

#if defined(CONFIG_FIRMWARE_USE_WEB)
typedef struct web_message {
    MessageType messageType;
    uint8_t volumeLevel;
    float temperature;
    float humidity;
} WebControlMessage;
#endif

typedef struct sensor_message {
    MessageType messageType;
    float temperature;
    float humidity;
} SensorMessage;

typedef struct led_message {
    MessageType messageType;
    bool state;
} LEDMessage;
