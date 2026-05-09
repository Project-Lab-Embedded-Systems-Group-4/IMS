#include "test_gpio.h"
#include "espidf-drivers/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ims-mcu-driver/gpio.h"
#include <stdio.h>

#define TEST_GPIO_PIN 2

void test_gpio_run(void) {
    printf("Starting GPIO Test...\n");
    /* Initialize IMS GPIO Driver */
    static struct ims_device gpio_dev = {.name = "esp-gpio"};
    static struct espidf_gpio_config gpio_config;
    static struct espidf_gpio_data gpio_data;

    // Configure pin 2 as output
    gpio_config.pins[TEST_GPIO_PIN].enable = true;
    gpio_config.pins[TEST_GPIO_PIN].flags = IMS_GPIO_FLAGS_OUTPUT;

    esp_err_t err = espidf_gpio_init(&gpio_dev, &gpio_config, &gpio_data);
    if (err != ESP_OK) {
        printf("Failed to initialize GPIO driver: %d\n", err);
        return;
    }
    printf("GPIO driver initialized successfully\n");

    int level = 0;
    for (int i = 10; i >= 0; i--) {
        printf("Toggling GPIO %d to %d... (%d seconds left)\n", TEST_GPIO_PIN,
               level, i);
        ims_gpio_port_pin_set(&gpio_dev, TEST_GPIO_PIN,
                              level ? IMS_GPIO_LEVEL_HIGH : IMS_GPIO_LEVEL_LOW);
        level = !level;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
