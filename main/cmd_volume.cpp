#include "pch.hpp"


#if defined(USE_CONSOLE)

/**
 * Global arguments
 *
 */

static struct {
    struct arg_int *level;
    struct arg_lit *up;
    struct arg_lit *down;
    struct arg_end *end;
} volume_args;


/**
 * Command handler
 *
 */

static int volume(int argc, char **argv) {
    if (0 != arg_parse(argc, argv, (void **)&volume_args)) {
        arg_print_errors(stderr, volume_args.end, argv[0]);
        return 1;
    }
    if (volume_args.up->count > 0) {
        volume_up();
    } else if (volume_args.down->count > 0) {
        volume_down();
    } else if (volume_args.level->count > 0) {
        int level = volume_args.level->ival[0];
        volume_set(level);
    }

    return 0;
}


/**
 * Command registration
 *
 */

void register_volume() {
    volume_args.level = arg_int0("l", "level", "<l>", "Volume level to set");
    volume_args.up = arg_litn("u", "up", 0, 1, "Volume up");
    volume_args.down = arg_litn("d", "down", 0, 1, "Volume down");
    volume_args.end = arg_end(2);

    const esp_console_cmd_t volume_cmd = {.command = "volume",
        .help = "Set volume level",
        .hint = nullptr,
        .func = &volume,
        .argtable = &volume_args};

    ESP_ERROR_CHECK(esp_console_cmd_register(&volume_cmd));
}

#endif