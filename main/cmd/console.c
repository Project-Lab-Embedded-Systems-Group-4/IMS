#include <stdio.h>
#include <string.h>
#include "esp_console.h"
#include "linenoise/linenoise.h"
#include "console.h"
#include "cmd.h"

static void my_console_completion(const char *buf, linenoiseCompletions *lc) {
    // 1. Call default console completions for top-level commands
    esp_console_get_completion(buf, lc);

    // 2. Parse the buffer to find the command and current typing state
    char temp_buf[256];
    strncpy(temp_buf, buf, sizeof(temp_buf) - 1);
    temp_buf[sizeof(temp_buf) - 1] = '\0';

    char *words[16];
    int word_count = 0;
    char *token = strtok(temp_buf, " ");
    while (token != NULL && word_count < 16) {
        words[word_count++] = token;
        token = strtok(NULL, " ");
    }

    if (word_count == 0) {
        return;
    }

    // Check if the command matches a parent command with registered subcommands
    const esp_console_subcmd_table_t *table = esp_console_get_subcommand_table(words[0]);
    if (!table) {
        return;
    }

    bool has_trailing_space = (buf[strlen(buf) - 1] == ' ');

    if (word_count == 1 && has_trailing_space) {
        // User typed "<cmd> " (with trailing space) and pressed TAB
        // Suggest all subcommands
        for (size_t i = 0; i < table->subcmd_count; i++) {
            char completed[128];
            snprintf(completed, sizeof(completed), "%s %s", words[0], table->subcmds[i].name);
            linenoiseAddCompletion(lc, completed);
        }
    } else if (word_count == 2 && !has_trailing_space) {
        // User is typing the subcommand name
        const char *prefix = words[1];
        size_t prefix_len = strlen(prefix);
        for (size_t i = 0; i < table->subcmd_count; i++) {
            if (strncmp(table->subcmds[i].name, prefix, prefix_len) == 0) {
                char completed[128];
                snprintf(completed, sizeof(completed), "%s %s", words[0], table->subcmds[i].name);
                linenoiseAddCompletion(lc, completed);
            }
        }
    } else if (word_count == 2 && has_trailing_space) {
        // User typed "<cmd> <subcmd> " (with trailing space) and pressed TAB
        // Show help/options for the subcommand
        const char *typed_subcmd = words[1];
        const esp_console_subcmd_t *matched_sub = NULL;
        for (size_t i = 0; i < table->subcmd_count; i++) {
            if (strcmp(table->subcmds[i].name, typed_subcmd) == 0) {
                matched_sub = &table->subcmds[i];
                break;
            }
        }

        if (matched_sub) {
            printf("\n\nUsage: %s %s", words[0], matched_sub->name);
            if (matched_sub->argtable) {
                arg_print_syntax(stdout, matched_sub->argtable, "\n");
                printf("\nOptions:\n");
                arg_print_glossary(stdout, matched_sub->argtable, "  %-25s %s\n");
            } else {
                printf("\n");
            }
            if (matched_sub->help) {
                printf("\nDescription:\n  %s\n", matched_sub->help);
            }
            printf("\nims> %s", buf);
            fflush(stdout);
        }
    }
}

void init_console(void) {
    /* Initialize Console REPL */
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "ims> ";
    repl_config.max_history_len = 10;

    esp_console_dev_uart_config_t uart_config =
        ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();

    /* Initialize console REPL environment */
    ESP_ERROR_CHECK(
        esp_console_new_repl_uart(&uart_config, &repl_config, &repl));

    /* Override linenoise completion callback */
    linenoiseSetCompletionCallback(&my_console_completion);

    /* Register Commands */
    register_system();
    register_i2c_tool_command();
    register_gpio_pin_command(40); // Max GPIOs
    register_ad5933_command();
    register_ad7680_command();
    register_board_utils_command();

    /* Start console REPL */
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
