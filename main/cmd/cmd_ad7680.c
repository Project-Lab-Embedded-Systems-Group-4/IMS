#include <argtable3/argtable3.h>
#include <esp_console.h>
#include <stdio.h>
#include <string.h>

#include "board/board.h"
#include "event.h"
#include "cmd.h"
#include "services/ad7680/ad7680_service.h"

static struct {
    struct arg_int *iterations;
    struct arg_end *end;
} read_args;

static int do_ad7680_read(int iterations) {
    extern esp_event_loop_handle_t service_event_loop;
    uint16_t iters = (uint16_t)iterations;
    esp_err_t err = esp_event_post_to(
        service_event_loop, IMS_EVENT_BASE, IMS_EVENT_AD7680_START_READ,
        &iters, sizeof(iters), portMAX_DELAY);
    if (err == ESP_OK) {
        printf("AD7680 Read requested (iterations: %d)...\n", iterations);
    } else {
        printf("Failed to request AD7680 read: %s\n", esp_err_to_name(err));
    }
    return 0;
}

static int do_ad7680_read_cmd(int argc, char **argv) {
    int iters = 1;
    if (read_args.iterations->count > 0) {
        iters = read_args.iterations->ival[0];
    }
    return do_ad7680_read(iters);
}

static int do_ad7680_cmd(int argc, char **argv) {
    return esp_console_dispatch_subcommand("ad7680", argc, argv);
}

esp_err_t register_ad7680_command(void) {
    read_args.iterations =
        arg_int0("i", "iterations", "<n>", "Number of iterations for averaging");
    read_args.end = arg_end(1);

    static const esp_console_subcmd_t subcmds[] = {
        { .name = "read", .help = "Read analog input from AD7680", .func = &do_ad7680_read_cmd, .argtable = &read_args }
    };

    ESP_ERROR_CHECK(esp_console_register_subcommands("ad7680", subcmds, sizeof(subcmds) / sizeof(subcmds[0])));

    const esp_console_cmd_t cmd = {
        .command = "ad7680",
        .help = "AD7680 ADC control commands",
        .hint = NULL,
        .func = &do_ad7680_cmd,
        .argtable = NULL,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));

    return ESP_OK;
}
