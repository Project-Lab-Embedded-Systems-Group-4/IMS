#ifndef ESP_IMS_MCU_DRIVER_ESPIDF_DRIVERS_GPIO_H_
#define ESP_IMS_MCU_DRIVER_ESPIDF_DRIVERS_GPIO_H_

#include <esp_err.h>
#include <hal/gpio_types.h>

#include "ims-mcu-driver/gpio.h"

struct espidf_gpio_pin_config {
    ims_gpio_flags_t flags;
    bool enable;
};

struct espidf_gpio_config {
    struct espidf_gpio_pin_config pins[GPIO_NUM_MAX];
};

struct espidf_gpio_data {};

esp_err_t espidf_gpio_init(struct ims_device *dev,
                           const struct espidf_gpio_config *config,
                           struct espidf_gpio_data *data);

#endif // ESP_IMS_MCU_DRIVER_ESPIDF_DRIVERS_GPIO_H_
