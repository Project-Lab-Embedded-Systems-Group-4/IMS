/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"

#include "espidf-drivers/gpio.h"
#include "ims-mcu-driver/gpio.h"

#define TEST_GPIO_PIN 2

void app_main(void)
{
    printf("Hello world!\n");

    /* Initialize IMS GPIO Driver */
    static struct ims_device gpio_dev = { .name = "esp-gpio" };
    static struct espidf_gpio_config gpio_config;
    static struct espidf_gpio_data gpio_data;

    // Configure pin 2 as output
    gpio_config.pins[TEST_GPIO_PIN].enable = true;
    gpio_config.pins[TEST_GPIO_PIN].flags = IMS_GPIO_FLAGS_OUTPUT;

    esp_err_t err = espidf_gpio_init(&gpio_dev, &gpio_config, &gpio_data);
    if (err != ESP_OK) {
        printf("Failed to initialize GPIO driver: %d\n", err);
    } else {
        printf("GPIO driver initialized successfully\n");
    }

    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    int level = 0;
    for (int i = 10; i >= 0; i--) {
        printf("Toggling GPIO %d to %d... (%d seconds left)\n", TEST_GPIO_PIN, level, i);
        ims_gpio_port_pin_set(&gpio_dev, TEST_GPIO_PIN, level ? IMS_GPIO_LEVEL_HIGH : IMS_GPIO_LEVEL_LOW);
        level = !level;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
