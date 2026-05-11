#include <argtable3/argtable3.h>
#include <esp_console.h>
#include <stdio.h>
#include <string.h>

#include "board/board.h"
#include "event.h"
#include "cmd.h"
#include "services/ad7680/ad7680_service.h"

static struct {
    struct arg_str *subcommand;
    struct arg_int *iterations;
    struct arg_end *end;
} ad7680_args;

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

static int do_ad7680_cmd(int argc, char **argv) {
    PARSE_ARG(ad7680_args);

    const char *sub = ad7680_args.subcommand->sval[0];

    if (strcmp(sub, "read") == 0) {
        int iters = 1;
        if (ad7680_args.iterations->count > 0) {
            iters = ad7680_args.iterations->ival[0];
        }
        return do_ad7680_read(iters);
    } else {
        printf("Unknown subcommand: %s. Use 'read'.\n", sub);
        return 1;
    }
}

esp_err_t register_ad7680_command(void) {
    ad7680_args.subcommand =
        arg_str1(NULL, NULL, "<read>", "Sub-command to execute");
    ad7680_args.iterations =
        arg_int0("i", "iterations", "<n>", "Number of iterations for averaging");
    ad7680_args.end = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command = "ad7680",
        .help = "AD7680 ADC control commands",
        .hint = NULL,
        .func = &do_ad7680_cmd,
        .argtable = &ad7680_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));

    return ESP_OK;
}
