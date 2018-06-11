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

typedef struct ipc_message {
    MessageType messageType;
    char text[3];
    uint8_t volumeLevel;
    float temperature;
    float humidity;
    bool led_state;
} IPCMessage;
