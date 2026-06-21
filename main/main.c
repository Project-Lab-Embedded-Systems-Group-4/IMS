/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_chip_info.h"
#include "esp_console.h"
#include "esp_event.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "nvs_flash.h"
#include "linenoise/linenoise.h"

#include "board/board.h"
#include "cmd.h"
#include "cmd/console.h"
#include "event.h"
#include "service.h"
#include "services/ad5933/ad5933_service.h"
#include "services/ad7680/ad7680_service.h"

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

#define TAG "main"

static esp_event_loop_args_t service_event_loop_args = {
    .queue_size = 64,
    .task_name = "srv-evloop",
    .task_priority = 4,
    .task_stack_size = 4096,
    .task_core_id = 0,
};

esp_event_loop_handle_t service_event_loop;

static esp_err_t event_loop_init(void) {
    return esp_event_loop_create(&service_event_loop_args, &service_event_loop);
}



void app_main(void) {
    ESP_LOGI(TAG, "Starting IMS Application");

    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    board_init();

    ESP_ERROR_CHECK(event_loop_init());

    init_console();

    /* Start AD5933 Service */
    static struct service ad5933_srv;
    static struct ad5933_service_config ad5933_cfg;
    ad5933_cfg.loop = service_event_loop;
    ad5933_cfg.ad5933_dev = board_get_device("ad5933"); // Ensure this is in board.c

    if (ad5933_cfg.ad5933_dev) {
        ESP_ERROR_CHECK(ad5933_service_init(&ad5933_srv, &ad5933_cfg));
        ESP_LOGI(TAG, "AD5933 Service started");
    } else {
        ESP_LOGW(TAG, "AD5933 device not found in board config");
    }

    /* Start AD7680 Service */
    static struct service ad7680_srv;
    static struct ad7680_service_config ad7680_cfg;
    ad7680_cfg.loop = service_event_loop;
    ad7680_cfg.ad7680_dev = board_get_device("ad7680");

    if (ad7680_cfg.ad7680_dev) {
        ESP_ERROR_CHECK(ad7680_service_init(&ad7680_srv, &ad7680_cfg));
        ESP_LOGI(TAG, "AD7680 Service started");
    } else {
        ESP_LOGW(TAG, "AD7680 device not found in board config");
    }

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
}
