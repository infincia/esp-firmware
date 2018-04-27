#include "pch.hpp"


#if defined(USE_CONSOLE)

#include "sdkconfig.h"

static void register_free();
static void register_restart();
static void register_save_wifi_credentials();
static void register_provision_device();

void register_system() {
    register_free();
    register_restart();
    register_save_wifi_credentials();
    register_provision_device();
}

/** 'restart' command restarts the program */

static int restart(int argc, char **argv) {
    printf("%s\n", "restarting...");
    esp_restart();
}

static void register_restart() {
    const esp_console_cmd_t cmd = {
        .command = "restart",
        .help = "restart the device",
        .hint = nullptr,
        .func = &restart,
        .argtable = nullptr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

/** 'free' command prints available heap memory */

static int free_mem(int argc, char **argv) {
    printf("%d\n", esp_get_free_heap_size());
    return 0;
}

static void register_free() {
    const esp_console_cmd_t cmd = {
        .command = "free",
        .help = "available heap memory",
        .hint = nullptr,
        .func = &free_mem,
        .argtable = nullptr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}


/** wifi commands */

static struct {
    struct arg_str *ssid;
    struct arg_str *password;
    struct arg_end *end;
} save_wifi_credentials_args;

static int save_wifi_credentials(int argc, char **argv) {
    if (0 != arg_parse(argc, argv, (void **)&save_wifi_credentials_args)) {
        arg_print_errors(stderr, save_wifi_credentials_args.end, argv[0]);
        return 1;
    }

    if (save_wifi_credentials_args.ssid->count > 0 && save_wifi_credentials_args.password->count > 0) {
        const char* _ssid = save_wifi_credentials_args.ssid->sval[0];
        const char* _password = save_wifi_credentials_args.password->sval[0];

        if (set_kv(WIFI_SSID_KEY, _ssid) && set_kv(WIFI_PASSWORD_KEY, _password)) {
            ESP_LOGI("[Console]", "wifi credentials saved, restarting");
            esp_restart();
        } else {
            ESP_LOGW("[Console]", "wifi credentials could not be saved");
        }
    }

    return 0;
}

static void register_save_wifi_credentials() {
    save_wifi_credentials_args.ssid = arg_strn("s", "ssid", "<ssid>", 1, 1, "SSID to connect to");
    save_wifi_credentials_args.password = arg_strn("p", "password", "<password>", 1, 1, "Password");
    save_wifi_credentials_args.end = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command = "save-wifi",
        .help = "Save wifi credentials",
        .hint = nullptr,
        .func = &save_wifi_credentials,
        .argtable = &save_wifi_credentials_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

/** device provisioning commands */

static struct {
    struct arg_str *name;
    struct arg_str *type;
    struct arg_end *end;
} provision_device_args;

static int provision_device(int argc, char **argv) {
    if (0 != arg_parse(argc, argv, (void **)&provision_device_args)) {
        arg_print_errors(stderr, provision_device_args.end, argv[0]);
        return 1;
    }

    if (provision_device_args.name->count > 0 && provision_device_args.type->count > 0) {
        const char* _name = provision_device_args.name->sval[0];
        const char* _type = provision_device_args.type->sval[0];

        if (set_kv(DEVICE_PROVISIONING_NAME_KEY, _name) && set_kv(DEVICE_PROVISIONING_TYPE_KEY, _type)) {
            ESP_LOGI("[Console]", "device provisioned, restarting");
            esp_restart();
        } else {
            ESP_LOGW("[Console]", "device provisioning could not be saved");
        }
    }

    return 0;
}

static void register_provision_device() {
    provision_device_args.name = arg_strn("n", "name", "<name>", 1, 1, "Name of device");
    provision_device_args.type = arg_strn("t", "type", "<sensor/amp/camera>", 1, 1, "Type of device");
    provision_device_args.end = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command = "provision-device",
        .help = "Provision device name and type",
        .hint = nullptr,
        .func = &provision_device,
        .argtable = &provision_device_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

#endif