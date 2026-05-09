/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <inttypes.h>
#include <stdio.h>

#ifdef GPIO_TEST
#include "test_gpio.h"
#endif

#ifdef I2C_TEST
#include "test_i2c.h"
#endif

#ifdef SPI_TEST
#include "test_spi.h"
#endif

#ifdef UART_TEST
#include "test_uart.h"
#endif

#include "board/board.h"
#include "cmd.h"
#include "esp_console.h"

static void init_console(void) {
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

    /* Register Commands */
    register_system();
    register_gpio_pin_command(40); // Max GPIOs

    /* Start console REPL */
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

void app_main(void) {
    board_init();

    init_console();

#ifdef GPIO_TEST
    test_gpio_run();
#endif

#ifdef I2C_TEST
    test_i2c_run();
#endif

#ifdef SPI_TEST
    test_spi_run();
#endif

#ifdef UART_TEST
    test_uart_run();
#endif

    printf("App main finished. Console is running...\n");
}
