#include <argtable3/argtable3.h>
#include <esp_console.h>
#include <stdio.h>
#include <string.h>

#include "cmd.h"
#include "board_utils.h"

static struct {
    struct arg_end *end;
} board_info_args;

static int do_board_info(int argc, char **argv) {
    struct board_utils_info info;
    board_utils_get_info(&info);

    printf("Current Board Utility Status:\n");
    printf("\tMeasurement Enable: %s\n", info.measure_enabled ? "ON" : "OFF");
    
    if (info.subj_channel != -1) {
        printf("\tSubject Channel:    %d\n", info.subj_channel);
    } else {
        printf("\tSubject Channel:    Not set\n");
    }

    if (info.zm_fb_index != -1) {
        uint32_t zm_fb[] = ZM_FB_RES_VALUES;
        printf("\tZM Feedback Index:  %d (%lu Ohm)\n", info.zm_fb_index, zm_fb[info.zm_fb_index]);
    } else {
        printf("\tZM Feedback Index:  Not set\n");
    }

    if (info.rm_range_index != -1) {
        uint32_t rm_ref[] = RM_REF_RES_VALUES;
        printf("\tRM Range Index:     %d (%lu Ohm ref)\n", info.rm_range_index, rm_ref[info.rm_range_index]);
    } else {
        printf("\tRM Range Index:     Not set\n");
    }

    return 0;
}

static struct {
    struct arg_int *subj;
    struct arg_int *fb;
    struct arg_int *rm;
    struct arg_int *enable;
    struct arg_end *end;
} board_set_args;

static int do_board_set(int argc, char **argv) {
    PARSE_ARG(board_set_args);

    if (board_set_args.subj->count) {
        board_set_subj_channel((enum board_subj_channel)board_set_args.subj->ival[0]);
        printf("Subject channel set to %d\n", board_set_args.subj->ival[0]);
    }

    if (board_set_args.fb->count) {
        board_set_zm_fb((uint8_t)board_set_args.fb->ival[0]);
        printf("ZM Feedback index set to %d\n", board_set_args.fb->ival[0]);
    }

    if (board_set_args.rm->count) {
        board_set_rm_range((uint8_t)board_set_args.rm->ival[0]);
        printf("RM Range index set to %d\n", board_set_args.rm->ival[0]);
    }

    if (board_set_args.enable->count) {
        board_measure_enable(board_set_args.enable->ival[0] ? true : false);
        printf("Measurement Enable set to %s\n", board_set_args.enable->ival[0] ? "ON" : "OFF");
    }

    return 0;
}

esp_err_t register_board_utils_command(void) {
    board_info_args.end = arg_end(0);
    const esp_console_cmd_t info_cmd = {
        .command = "board_info",
        .help = "Show current board MUX and Enable status",
        .hint = NULL,
        .func = &do_board_info,
        .argtable = &board_info_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&info_cmd));

    board_set_args.subj = arg_int0("s", "subj", "<0-15>", "Set subject channel (10,12,14 are CAL)");
    board_set_args.fb   = arg_int0("f", "fb", "<0-3>", "Set ZM feedback resistor index");
    board_set_args.rm   = arg_int0("r", "rm", "<0-3>", "Set RM reference resistor index");
    board_set_args.enable = arg_int0("e", "enable", "<0|1>", "Enable(1) or Disable(0) measurement");
    board_set_args.end  = arg_end(4);

    const esp_console_cmd_t set_cmd = {
        .command = "board_set",
        .help = "Configure board MUX and Enable lines",
        .hint = NULL,
        .func = &do_board_set,
        .argtable = &board_set_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_cmd));

    return ESP_OK;
}
