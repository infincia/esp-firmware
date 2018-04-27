#include "pch.hpp"


#if defined(USE_CONSOLE)

#include "console.hpp"

static const char *TAG = "[Console]";

#define CONFIG_STORE_HISTORY 0


/**
 * @brief Task wrapper
 */

static void task_wrapper(void *param) {
	auto *instance = static_cast<Console *>(param);
    instance->task();
}


/**
 * @brief Console
 */

Console::Console() {
    /* Disable buffering on stdin and stdout */
    setvbuf(stdin, nullptr, _IONBF, 0);
    setvbuf(stdout, nullptr, _IONBF, 0);

    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    esp_vfs_dev_uart_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    esp_vfs_dev_uart_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);

    /* Install UART driver for interrupt-driven reads and writes */
    ESP_ERROR_CHECK(uart_driver_install((uart_port_t)CONFIG_CONSOLE_UART_NUM, 256, 0, 0, nullptr, 0));

    /* Tell VFS to use UART driver */
    esp_vfs_dev_uart_use_driver((uart_port_t)CONFIG_CONSOLE_UART_NUM);

    /* Initialize the console */
    esp_console_config_t console_config;
    console_config.max_cmdline_args = 8;
    console_config.max_cmdline_length = 256;
#if CONFIG_LOG_COLORS
    console_config.hint_color = atoi(LOG_COLOR_CYAN);
#endif

    ESP_ERROR_CHECK(esp_console_init(&console_config));

    /* Configure linenoise line completion library */
    /* Enable multiline editing. If not set, long commands will scroll within
     * single line.
     */
    linenoiseSetMultiLine(1);

    /* Tell linenoise where to get command completions and hints */
    linenoiseSetCompletionCallback(&esp_console_get_completion);
    linenoiseSetHintsCallback((linenoiseHintsCallback *)&esp_console_get_hint);

    /* Set command history size */
    linenoiseHistorySetMaxLen(100);

    xTaskCreate(&task_wrapper, "console_task", 8192, this, (tskIDLE_PRIORITY + 10), &this->console_task_handle);
}


Console::~Console() = default;


void Console::task() {
    /* Wait for filesystem to be available */
    //xEventGroupWaitBits(filesystem_event_group, FS_READY_BIT, false, true, portMAX_DELAY);


    #if CONFIG_STORE_HISTORY
        /* Load command history from filesystem */
        linenoiseHistoryLoad(HISTORY_PATH);
    #endif


    /* Register commands */
    esp_console_register_help_command();
    register_system();
    register_volume();    

    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    const char *prompt = LOG_COLOR_I "# " LOG_RESET_COLOR;

    printf("\n"
           "Type 'help' to get the list of commands\n\n");

    /* Figure out if the terminal supports escape sequences */
    int probe_status = linenoiseProbe();
    if (probe_status) { /* zero indicates success */
        printf("\n"
               "Escape sequences unavailable.\n");
        linenoiseSetDumbMode(1);
#if CONFIG_LOG_COLORS
        /* Since the terminal doesn't support escape sequences,
         * don't use color codes in the prompt.
         */
        prompt = "# ";
#endif // CONFIG_LOG_COLORS
    }

    /* Main loop */
    while (true) {
        /* Get a line using linenoise.
         * The line is returned when ENTER is pressed.
         */
        char *line = linenoise(prompt);
        if (line == nullptr) { /* Ignore empty lines */
            continue;
        }
        /* Add the command to the history */
        linenoiseHistoryAdd(line);

        /* Save command history to filesystem */
        linenoiseHistorySave(HISTORY_PATH);

        /* Try to run the command */
        int ret;
        esp_err_t err = esp_console_run(line, &ret);
        if (err == ESP_ERR_NOT_FOUND) {
            printf("Invalid command\n");
        } else if (err == ESP_ERR_INVALID_ARG) {
            // command was empty
        } else if (err == ESP_OK && ret != ESP_OK) {
            printf("Warning: 0x%x (%d)\n", ret, err);
        } else if (err != ESP_OK) {
            printf("Error: %d\n", err);
        }
        /* linenoise allocates line buffer on the heap, so need to free it */
        linenoiseFree(line);
    }
}

#endif