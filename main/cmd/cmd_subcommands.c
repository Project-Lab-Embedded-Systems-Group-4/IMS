#include <string.h>
#include <stdio.h>
#include <esp_err.h>
#include <argtable3/argtable3.h>
#include "cmd.h"

#define MAX_PARENT_CMDS 16
static esp_console_subcmd_table_t s_subcmd_tables[MAX_PARENT_CMDS];
static int s_subcmd_tables_count = 0;

esp_err_t esp_console_register_subcommands(const char *parent_cmd, const esp_console_subcmd_t *subcmds, size_t subcmd_count) {
    if (s_subcmd_tables_count >= MAX_PARENT_CMDS) {
        return ESP_ERR_NO_MEM;
    }
    s_subcmd_tables[s_subcmd_tables_count].parent_cmd = parent_cmd;
    s_subcmd_tables[s_subcmd_tables_count].subcmds = subcmds;
    s_subcmd_tables[s_subcmd_tables_count].subcmd_count = subcmd_count;
    s_subcmd_tables_count++;
    return ESP_OK;
}

const esp_console_subcmd_table_t *esp_console_get_subcommand_table(const char *parent_cmd) {
    for (int i = 0; i < s_subcmd_tables_count; i++) {
        if (strcmp(s_subcmd_tables[i].parent_cmd, parent_cmd) == 0) {
            return &s_subcmd_tables[i];
        }
    }
    return NULL;
}

int esp_console_dispatch_subcommand(const char *parent_cmd, int argc, char **argv) {
    const esp_console_subcmd_table_t *table = esp_console_get_subcommand_table(parent_cmd);
    if (!table) {
        printf("Error: no subcommands registered for '%s'\n", parent_cmd);
        return 1;
    }

    if (argc < 2 || strcmp(argv[1], "help") == 0) {
        printf("Usage: %s <subcommand> [options]\n\nAvailable subcommands:\n", argv[0]);
        for (size_t i = 0; i < table->subcmd_count; i++) {
            printf("  %-15s %s\n", table->subcmds[i].name, table->subcmds[i].help ? table->subcmds[i].help : "");
        }
        return 0;
    }

    const char *subcmd_name = argv[1];
    const esp_console_subcmd_t *matched_sub = NULL;
    for (size_t i = 0; i < table->subcmd_count; i++) {
        if (strcmp(table->subcmds[i].name, subcmd_name) == 0) {
            matched_sub = &table->subcmds[i];
            break;
        }
    }

    if (!matched_sub) {
        printf("Unknown subcommand '%s'. Available subcommands:\n", subcmd_name);
        for (size_t i = 0; i < table->subcmd_count; i++) {
            printf("  %-15s %s\n", table->subcmds[i].name, table->subcmds[i].help ? table->subcmds[i].help : "");
        }
        return 1;
    }

    if (matched_sub->argtable) {
        // Parse options starting from argv[1]
        // Note: arg_parse expects argv[0] to be the command name, which argv[1] ("sweep", "cal", etc.) is.
        int err = arg_parse(argc - 1, argv + 1, matched_sub->argtable);
        if (err != 0) {
            // Find the arg_end struct in the table
            void **table_arr = (void **)matched_sub->argtable;
            struct arg_end *end_hdr = NULL;
            for (int idx = 0; table_arr[idx] != NULL; idx++) {
                struct arg_hdr *hdr = (struct arg_hdr *)table_arr[idx];
                if (hdr->flag & 0x01 /* ARG_TERMINATOR */) {
                    end_hdr = (struct arg_end *)hdr;
                    break;
                }
            }
            if (end_hdr) {
                arg_print_errors(stderr, end_hdr, matched_sub->name);
            } else {
                printf("Argument parsing failed.\n");
            }
            return 1;
        }
    }

    return matched_sub->func(argc - 1, argv + 1);
}
