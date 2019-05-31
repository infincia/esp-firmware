#pragma once

typedef enum message_type {
    ControlMessageTypeVolumeUp,
    ControlMessageTypeVolumeDown,
    ControlMessageTypeVolumeSet,
    ControlMessageTypeDisplayText,
    ControlMessageTypeVolumeEvent,
    EventTemperatureSensorValue,
    EventTemperatureHeaterControl,
    EventIdentifyLED,
    EventErrorLED,
    EventReadyLED
} MessageType;

typedef struct ipc_message {
    MessageType messageType;
    char text[3];
    uint8_t volumeLevel;
    float temperature;
    float humidity;
    bool led_state;
    bool heater_state;
    uint8_t heater_level;
} IPCMessage;
