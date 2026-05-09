#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_log.h>

#include "espidf-drivers/gpio.h"
#include "ims-mcu-driver/gpio.h"

#define TAG "esp-gpio"

static int espidf_gpio_pin_set_input(const struct ims_device *dev,
                                     ims_gpio_pin_t pin, ims_gpio_flags_t flags) {
    esp_err_t err = gpio_set_direction(pin, GPIO_MODE_INPUT);
    if (err != ESP_OK) {
        return -EIO;
    }

    gpio_pull_mode_t mode = GPIO_FLOATING;
    if ((flags & IMS_GPIO_FLAGS_INPUT_PULL_UP) &&
        (flags & IMS_GPIO_FLAGS_INPUT_PULL_DOWN)) {
        mode = GPIO_PULLUP_PULLDOWN;
    } else if (flags & IMS_GPIO_FLAGS_INPUT_PULL_UP) {
        mode = GPIO_PULLUP_ONLY;
    } else if (flags & IMS_GPIO_FLAGS_INPUT_PULL_DOWN) {
        mode = GPIO_PULLDOWN_ONLY;
    }

    err = gpio_set_pull_mode(pin, mode);
    if (err != ESP_OK) {
        return -EIO;
    }
    return 0;
}

static int espidf_gpio_pin_set_output(const struct ims_device *dev,
                                      ims_gpio_pin_t pin,
                                      ims_gpio_flags_t flags) {
    esp_err_t err = ESP_OK;
    if (flags & IMS_GPIO_FLAGS_LINE_OPEN_DRAIN) {
        err = gpio_set_direction(pin, GPIO_MODE_OUTPUT_OD);
    } else {
        err = gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    }
    if (err != ESP_OK) {
        return -EIO;
    }

    gpio_pulldown_dis(pin);
    gpio_pullup_dis(pin);

    if (flags & IMS_GPIO_FLAGS_OUTPUT_INIT_LOW) {
        err = gpio_set_level(pin, 0);
    } else if (flags & IMS_GPIO_FLAGS_OUTPUT_INIT_HIGH) {
        err = gpio_set_level(pin, 1);
    }
    if (err != ESP_OK) {
        return -EIO;
    }

    return 0;
}

static int espidf_gpio_pin_get_config(const struct ims_device *dev,
                                      ims_gpio_pin_t pin,
                                      ims_gpio_flags_t *flags) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    IMS_MCU_DRIVER_ASSERT(flags != NULL);

    const struct espidf_gpio_config *config = dev->config;
    if (!config->pins[pin].enable) {
        return -EINVAL;
    }
    *flags = config->pins[pin].flags;
    return 0;
}

static int espidf_gpio_pin_set_config(const struct ims_device *dev,
                                      ims_gpio_pin_t pin,
                                      ims_gpio_flags_t flags) {
    int err = 0;
    if ((flags & IMS_GPIO_FLAGS_INPUT) > 0) {
        err = espidf_gpio_pin_set_input(dev, pin, flags);
    } else if ((flags & IMS_GPIO_FLAGS_OUTPUT) > 0) {
        err = espidf_gpio_pin_set_output(dev, pin, flags);
    }
    return err;
}

static int espidf_gpio_pin_get(const struct ims_device *dev, ims_gpio_pin_t pin) {
    const struct espidf_gpio_config *config = dev->config;
    int ret = -1;

    if ((config->pins[pin].flags & IMS_GPIO_FLAGS_INPUT) > 0) {
        ret =
            (gpio_get_level(pin) == 1) ? IMS_GPIO_LEVEL_HIGH : IMS_GPIO_LEVEL_LOW;
    } else if ((config->pins[pin].flags & IMS_GPIO_FLAGS_OUTPUT) > 0) {
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) ||  \
    defined(CONFIG_IDF_TARGET_ESP32S3)
#if defined(CONFIG_IDF_TARGET_ESP32)
#include <esp32/rom/gpio.h>
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
#include <esp32s2/rom/gpio.h>
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#include <esp32s3/rom/gpio.h>
#endif
        if (pin >= 32)
            ret = (GPIO_REG_READ(GPIO_OUT1_REG) >> pin) & 1U;
        else
#else
#if defined CONFIG_IDF_TARGET_ESP32C3
#include <esp32c3/rom/gpio.h>
#endif
#endif
            ret = (GPIO_REG_READ(GPIO_OUT_REG) >> pin) & 1U;
    }

    return ret;
}

static int espidf_gpio_pin_set(const struct ims_device *dev, ims_gpio_pin_t pin,
                               int level) {
    int espidf_level = (level == IMS_GPIO_LEVEL_HIGH) ? 1 : 0;
    if (gpio_set_level(pin, espidf_level) != ESP_OK) {
        return -EIO;
    }
    return 0;
}

static int espidf_gpio_pin_interrupt_configure(const struct ims_device *dev,
                                               ims_gpio_pin_t pin,
                                               enum ims_gpio_int_mode mode,
                                               enum ims_gpio_int_trig trig) {
    esp_err_t esp_err;
    if (mode == IMS_GPIO_INT_MODE_DISABLED) {
        esp_err = gpio_intr_disable(pin);
    } else if (mode == IMS_GPIO_INT_MODE_EDGE && trig == IMS_GPIO_INT_TRIG_HIGH) {
        esp_err = gpio_set_intr_type(pin, GPIO_INTR_POSEDGE);
    } else if (mode == IMS_GPIO_INT_MODE_EDGE && trig == IMS_GPIO_INT_TRIG_LOW) {
        esp_err = gpio_set_intr_type(pin, GPIO_INTR_NEGEDGE);
    } else if (mode == IMS_GPIO_INT_MODE_EDGE && trig == IMS_GPIO_INT_TRIG_BOTH) {
        esp_err = gpio_set_intr_type(pin, GPIO_INTR_ANYEDGE);
    } else if (mode == IMS_GPIO_INT_MODE_LEVEL &&
               trig == IMS_GPIO_INT_TRIG_HIGH) {
        esp_err = gpio_set_intr_type(pin, GPIO_INTR_HIGH_LEVEL);
    } else if (mode == IMS_GPIO_INT_MODE_LEVEL && trig == IMS_GPIO_INT_TRIG_LOW) {
        esp_err = gpio_set_intr_type(pin, GPIO_INTR_LOW_LEVEL);
    } else {
        return -EINVAL;
    }
    if (esp_err != ESP_OK) {
        return -EIO;
    }

    if (mode != IMS_GPIO_INT_MODE_DISABLED) {
        esp_err = gpio_intr_enable(pin);
    }
    if (esp_err != ESP_OK) {
        return -EIO;
    }
    return 0;
}

static int espidf_gpio_pin_isr_register(const struct ims_device *dev,
                                        ims_gpio_pin_t pin,
                                        ims_gpio_pin_isr_t isr, void *arg) {
    esp_err_t err = gpio_isr_handler_add(pin, isr, arg);
    if (err != ESP_OK) {
        return -EIO;
    }
    return 0;
}

static const struct ims_gpio_port_driver_api espidf_gpio_api = {
    .pin_get_config = espidf_gpio_pin_get_config,
    .pin_set_config = espidf_gpio_pin_set_config,
    .pin_get = espidf_gpio_pin_get,
    .pin_set = espidf_gpio_pin_set,
    .pin_interrupt_configure = espidf_gpio_pin_interrupt_configure,
    .pin_isr_register = espidf_gpio_pin_isr_register,
};

esp_err_t espidf_gpio_init(struct ims_device *dev,
                           const struct espidf_gpio_config *config,
                           struct espidf_gpio_data *data) {
    dev->api = &espidf_gpio_api;
    dev->config = config;
    dev->data = data;

    for (int i = 0; i < sizeof(config->pins) / sizeof(config->pins[0]); i++) {
        ims_gpio_flags_t flags = config->pins[i].flags;
        ims_gpio_pin_t pin = (ims_gpio_pin_t)i;

        gpio_config_t io_conf;
        io_conf.pin_bit_mask = (1ULL << pin);
        if (flags & IMS_GPIO_FLAGS_INPUT) {
            io_conf.mode = GPIO_MODE_INPUT;
            io_conf.pull_down_en = (flags & IMS_GPIO_FLAGS_INPUT_PULL_DOWN)
                                       ? GPIO_PULLDOWN_ENABLE
                                       : GPIO_PULLDOWN_DISABLE;
            io_conf.pull_up_en = (flags & IMS_GPIO_FLAGS_INPUT_PULL_UP)
                                     ? GPIO_PULLUP_ENABLE
                                     : GPIO_PULLUP_DISABLE;
            io_conf.intr_type = GPIO_INTR_DISABLE;
            ESP_ERROR_CHECK(gpio_config(&io_conf));
        } else if ((flags & IMS_GPIO_FLAGS_OUTPUT) > 0) {
            io_conf.mode = GPIO_MODE_OUTPUT;
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
            io_conf.intr_type = GPIO_INTR_DISABLE;
            ESP_ERROR_CHECK(gpio_config(&io_conf));
        }
    }

    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio init: install gpio isr service");
        return err;
    }

    return ESP_OK;
}
