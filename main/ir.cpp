#include "pch.hpp"


#if defined(USE_AMP)


#include "ir.hpp"


static const char *TAG = "[IR]";


typedef struct {
    bool valid;
    uint8_t address;
    uint8_t command;
} NECValue;


/*
 * @brief RMT parameters
 */

static uint32_t base_clk();
static int clk_div();
static int tick_10_us();
static int timeout_us();
static int filter_tick_thresh();
static int idle_threshold();
static uint32_t duration_from_ticks(uint32_t ticks);


/*
 * @brief NEC decoding
 */

static bool isInRange(rmt_item32_t item, int lowDuration, int highDuration, int tolerance);
static bool NEC_is0(rmt_item32_t item);
static bool NEC_is1(rmt_item32_t item);
static bool decodeNEC(rmt_item32_t *data, int numItems, NECValue *value);


/**
 *
 * @brief IR task wrapper
 */

static void task_wrapper(void *param) {
	auto *instance = static_cast<IR *>(param);
    instance->task();
}


/**
 *
 * @brief IR
 */

IR::IR() {
    rmt_config_t config;
    config.rmt_mode = RMT_MODE_RX;
    config.channel = RMT_CHANNEL_0;
    config.gpio_num = (gpio_num_t)35;
    config.mem_block_num = 1;
    config.rx_config.filter_en = true;
    config.rx_config.filter_ticks_thresh = filter_tick_thresh();
    config.rx_config.idle_threshold = idle_threshold();
    config.clk_div = clk_div();

    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 1000, 0));

    xTaskCreate(&task_wrapper, "ir_task", 2048, this, (tskIDLE_PRIORITY + 10), &this->ir_task_handle);
}


IR::~IR() = default;


/**
 * @brief IR receiver task
 *
 */

void IR::task() {
    RingbufHandle_t ringBuf = NULL;
    rmt_get_ringbuf_handle(RMT_CHANNEL_0, &ringBuf);

    rmt_rx_start(RMT_CHANNEL_0, true);

    size_t itemSize;

    long last = millis();

    while (true) {
        void *data = xRingbufferReceive(ringBuf, &itemSize, portMAX_DELAY);
        if (data != nullptr) {
            /*
             * Ignore commands unless it has been longer than 125ms since last
             * command. This is a crude way of avoiding duplicates
             */
            long now = millis();

            if ((now - last) < 115) {
                vRingbufferReturnItem(ringBuf, data);
                continue;
            }

            last = millis();

            uint32_t numItems = itemSize / sizeof(rmt_item32_t);
            NECValue v;
            if (decodeNEC((rmt_item32_t *)data, numItems, &v)) {
                if (v.valid) {
                    if (v.address == 0xa6 && v.command == 0x0b) {
                        volume_down();
                    } else if (v.address == 0xa6 && v.command == 0x0a) {
                        volume_up();
                    }
                }
            }
            vRingbufferReturnItem(ringBuf, data);
            // vTaskDelay(125 / portTICK_RATE_MS);
        }
    }
}

/*
 * @brief RMT parameters
 */


static uint32_t base_clk() {
    uint32_t apb_freq = (rtc_clk_apb_freq_get() + 500000) / 1000000 * 1000000;
    return apb_freq;
}


static int clk_div() {
    // Clock divisor (base clock is 80MHz)
    return 100;
}


static int tick_10_us() {
    int b = base_clk();
    int d = clk_div();
    return (b / d / 100000); /*!< RMT counter value for 10 us. (Source clock is APB clock) */
}


static int timeout_us() {
    return 9500;
}


static int filter_tick_thresh() {
    // 80000000/100 -> 800000 / 100 = 8000  = 125us
    return 100;
}


static int idle_threshold() {
    int timeout = timeout_us();
    int tick = tick_10_us();
    return timeout / 10 * tick;
}


static uint32_t duration_from_ticks(uint32_t ticks) {
    return ((ticks & 0x7fff) * 10 / tick_10_us());
}

/*
 * @brief NEC decoding
 */

static bool isInRange(rmt_item32_t item, int lowDuration, int highDuration, int tolerance) {
    uint32_t lowValue = duration_from_ticks(item.duration0);
    uint32_t highValue = duration_from_ticks(item.duration1);
    /*
    ESP_LOGD(TAG, "lowValue=%d, highValue=%d, lowDuration=%d, highDuration=%d",
            lowValue, highValue, lowDuration, highDuration);
    */
    if (lowValue < (lowDuration - tolerance) || lowValue > (lowDuration + tolerance) ||
        (highValue != 0 &&
            (highValue < (highDuration - tolerance) || highValue > (highDuration + tolerance)))) {
        return false;
    }
    return true;
}


static bool NEC_is0(rmt_item32_t item) {
    return isInRange(item, 560, 560, 100);
}


static bool NEC_is1(rmt_item32_t item) {
    return isInRange(item, 560, 1690, 100);
}


static bool decodeNEC(rmt_item32_t *data, int numItems, NECValue *value) {
    value->valid = false;

    if (!isInRange(data[0], 9000, 4500, 200)) {
        ESP_LOGE(TAG, "Not an NEC packet");
        return false;
    }
    int i;
    uint8_t address = 0, notAddress = 0, command = 0, notCommand = 0;
    int accumCounter = 0;
    uint8_t accumValue = 0;
    for (i = 1; i < numItems; i++) {
        if (NEC_is0(data[i])) {
            // ESP_LOGD(TAG, "%d: 0", i);
            accumValue = accumValue >> 1;
        } else if (NEC_is1(data[i])) {
            // ESP_LOGD(TAG, "%d: 1", i);
            accumValue = (accumValue >> 1) | 0x80;
        } else {
            ESP_LOGD(TAG, "Unknown bit value");
        }
        if (accumCounter == 7) {
            accumCounter = 0;
            // ESP_LOGD(TAG, "Byte: 0x%.2x", accumValue);
            if (i == 8) {
                address = accumValue;
            } else if (i == 16) {
                notAddress = accumValue;
            } else if (i == 24) {
                command = accumValue;
            } else if (i == 32) {
                notCommand = accumValue;
            }
            accumValue = 0;
        } else {
            accumCounter++;
        }
    }
    ESP_LOGD(TAG, "Address: 0x%.2x, NotAddress: 0x%.2x", address, notAddress ^ 0xff);
    if (address != (notAddress ^ 0xff) || command != (notCommand ^ 0xff)) {
        ESP_LOGW(TAG, "Data mis match");
        return false;
    }
    // ESP_LOGD(TAG, "Address: 0x%.2x, Command: 0x%.2x", address, command);

    value->valid = true;

    value->address = address;
    value->command = command;

    return true;
}

#endif