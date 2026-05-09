#include <driver/gpio.h>
#include <driver/i2c.h>
#include <driver/spi_master.h>
#include <esp_err.h>
#include <esp_log.h>
#include <string.h>

#include "board/board.h"
#include "espidf-drivers/gpio.h"
#include "espidf-drivers/i2c.h"
#include "espidf-drivers/spi.h"
#include "ims-mcu-driver/device.h"
#include "ims-mcu-driver/gpio.h"
#include "ims-mcu-driver/i2c.h"
#include "ims-mcu-driver/spi.h"
#include "ims-mcu-driver/util.h"

#define TAG "board"

static struct board_info board_info = {
    .name = "GESTURE_ESP",
    .version = 1,
    .revision = 3,
};

static struct {
    struct ims_device dev;
    struct espidf_gpio_config config;
    struct espidf_gpio_data data;
} gpio_configs[] = {
    {
        .dev = {.name = "gpio"},
        .config =
            {
                .pins =
                    {
                        [GPIO_NUM_0] =
                            {
                                .enable = true,
                                .flags = IMS_GPIO_FLAGS_INPUT,
                            },
                        [GPIO_NUM_2] =
                            {
                                .enable = true,
                                .flags = IMS_GPIO_FLAGS_OUTPUT,
                            },
                        [GPIO_NUM_5] =
                            {
                                .enable = true,
                                .flags = IMS_GPIO_FLAGS_OUTPUT |
                                         IMS_GPIO_FLAGS_OUTPUT_INIT_HIGH,
                            },
                        [GPIO_NUM_16] =
                            {
                                .enable = true,
                                .flags = IMS_GPIO_FLAGS_OUTPUT,
                            },
                        [GPIO_NUM_17] =
                            {
                                .enable = true,
                                .flags = IMS_GPIO_FLAGS_OUTPUT,
                            },
                        [GPIO_NUM_21] =
                            {
                                .enable = true,
                                .flags = IMS_GPIO_FLAGS_OUTPUT,
                            },
                        [GPIO_NUM_22] =
                            {
                                .enable = true,
                                .flags = IMS_GPIO_FLAGS_OUTPUT,
                            },
                        [GPIO_NUM_25] =
                            {
                                .enable = true,
                                .flags = IMS_GPIO_FLAGS_OUTPUT,
                            },
                        [GPIO_NUM_26] =
                            {
                                .enable = true,
                                .flags = IMS_GPIO_FLAGS_OUTPUT,
                            },
                        [GPIO_NUM_27] =
                            {
                                .enable = true,
                                .flags = IMS_GPIO_FLAGS_OUTPUT,
                            },
                        [GPIO_NUM_32] =
                            {
                                .enable = true,
                                .flags = IMS_GPIO_FLAGS_OUTPUT,
                            },
                        [GPIO_NUM_33] =
                            {
                                .enable = true,
                                .flags = IMS_GPIO_FLAGS_OUTPUT,
                            },
                    },
            },
    },
};

static struct {
    struct ims_device dev;
    const char *gpio_bus_name;
    struct ims_gpio_pin_config config;
} gpio_pin_configs[] = {
    {
        .dev = {.name = "sys_led"},
        .gpio_bus_name = "gpio",
        .config = {.pin = GPIO_NUM_2},
    },
    {
        .dev = {.name = "button"},
        .gpio_bus_name = "gpio",
        .config = {.pin = GPIO_NUM_0},
    },
    {
        .dev = {.name = "zm_fb_a0"},
        .gpio_bus_name = "gpio",
        .config = {.pin = GPIO_NUM_21},
    },
    {
        .dev = {.name = "zm_fb_a1"},
        .gpio_bus_name = "gpio",
        .config = {.pin = GPIO_NUM_22},
    },
    {
        .dev = {.name = "subj_a0"},
        .gpio_bus_name = "gpio",
        .config = {.pin = GPIO_NUM_26},
    },
    {
        .dev = {.name = "subj_a1"},
        .gpio_bus_name = "gpio",
        .config = {.pin = GPIO_NUM_27},
    },
    {
        .dev = {.name = "subj_a2"},
        .gpio_bus_name = "gpio",
        .config = {.pin = GPIO_NUM_32},
    },
    {
        .dev = {.name = "subj_a3"},
        .gpio_bus_name = "gpio",
        .config = {.pin = GPIO_NUM_33},
    },
    {
        .dev = {.name = "rm_range_a0"},
        .gpio_bus_name = "gpio",
        .config = {.pin = GPIO_NUM_16},
    },
    {
        .dev = {.name = "rm_range_a1"},
        .gpio_bus_name = "gpio",
        .config = {.pin = GPIO_NUM_17},
    },
    {
        .dev = {.name = "measure_en"},
        .gpio_bus_name = "gpio",
        .config = {.pin = GPIO_NUM_25},
    },
};

static struct {
    struct ims_device dev;
    struct espidf_i2c_config config;
    struct espidf_i2c_data data;
} i2c_configs[] = {
    {
        .dev = {.name = "i2c-0"},
        .config =
            {
                .port = I2C_NUM_0,
                .config =
                    {
                        .mode = I2C_MODE_MASTER,
                        .sda_io_num = GPIO_NUM_15,
                        .sda_pullup_en = GPIO_PULLUP_ENABLE,
                        .scl_io_num = GPIO_NUM_4,
                        .scl_pullup_en = GPIO_PULLUP_ENABLE,
                        .master = {.clk_speed = 400000},
                    },
                .rxbuf_size = 0,
                .txbuf_size = 0,
            },
    },
};

static struct {
    struct ims_device dev;
    struct espidf_spidev_config config;
    struct espidf_spidev_data data;
} spi_configs[] = {
    {
        .dev = {.name = "spi-3"},
        .config =
            {
                .port = SPI3_HOST,
                .config =
                    {
                        .mode = 0,
                        .clock_speed_hz = 1000000,
                        .spics_io_num = GPIO_NUM_5,
                        .queue_size = 7,
                    },
            },
    },
};

static struct espidf_spibus_config spi_bus_config = {
    .port = SPI3_HOST,
    .config =
        {
            .mosi_io_num = GPIO_NUM_23,
            .miso_io_num = GPIO_NUM_19,
            .sclk_io_num = GPIO_NUM_18,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
        },
    .dma_chan = SPI_DMA_CH_AUTO,
};

static esp_err_t board_gpio_init(void) {
    for (int i = 0; i < ARRAY_SIZE(gpio_configs); i++) {
        ESP_ERROR_CHECK(espidf_gpio_init(&gpio_configs[i].dev,
                                         &gpio_configs[i].config,
                                         &gpio_configs[i].data));
    }
    return ESP_OK;
}

static esp_err_t board_gpio_pin_init(void) {
    for (int i = 0; i < ARRAY_SIZE(gpio_pin_configs); i++) {
        struct ims_device *dev = &gpio_pin_configs[i].dev;
        struct ims_gpio_pin_config *config = &gpio_pin_configs[i].config;
        config->dev = board_get_device(gpio_pin_configs[i].gpio_bus_name);
        ESP_ERROR_CHECK(ims_gpio_pin_init(dev, config));
    }
    return ESP_OK;
}

static esp_err_t board_i2c_init(void) {
    for (int i = 0; i < ARRAY_SIZE(i2c_configs); i++) {
        ESP_ERROR_CHECK(espidf_i2c_init(
            &i2c_configs[i].dev, &i2c_configs[i].config, &i2c_configs[i].data));
    }
    return ESP_OK;
}

static esp_err_t board_spi_init(void) {
    ESP_ERROR_CHECK(espidf_spibus_init(&spi_bus_config));
    for (int i = 0; i < ARRAY_SIZE(spi_configs); i++) {
        ESP_ERROR_CHECK(espidf_spidev_init(
            &spi_configs[i].dev, &spi_configs[i].config, &spi_configs[i].data));
    }
    return ESP_OK;
}

esp_err_t board_init(void) {
    ESP_ERROR_CHECK(board_gpio_init());
    ESP_ERROR_CHECK(board_gpio_pin_init());
    ESP_ERROR_CHECK(board_i2c_init());
    ESP_ERROR_CHECK(board_spi_init());
    return ESP_OK;
}

const struct ims_device *board_get_device(const char *name) {
#define FIND_DEVICE(device_configs, name)                                      \
    do {                                                                       \
        for (int i = 0; i < ARRAY_SIZE(device_configs); i++) {                 \
            if (strcmp(device_configs[i].dev.name, name) == 0) {               \
                return &device_configs[i].dev;                                 \
            }                                                                  \
        }                                                                      \
    } while (0)

    FIND_DEVICE(gpio_configs, name);
    FIND_DEVICE(gpio_pin_configs, name);
    FIND_DEVICE(i2c_configs, name);
    FIND_DEVICE(spi_configs, name);

#undef FIND_DEVICE
    return NULL;
}

int board_get_device_by_type(enum board_device_type type,
                             const struct ims_device *devs[], int size_devs) {
    int count = 0;
    if (type == BOARD_DEVICE_TYPE_ALL || type == BOARD_DEVICE_TYPE_GPIO_PORT) {
        for (int i = 0; i < ARRAY_SIZE(gpio_configs) && count < size_devs;
             i++) {
            devs[count++] = &gpio_configs[i].dev;
        }
    }
    if (type == BOARD_DEVICE_TYPE_ALL || type == BOARD_DEVICE_TYPE_GPIO_PIN) {
        for (int i = 0; i < ARRAY_SIZE(gpio_pin_configs) && count < size_devs;
             i++) {
            devs[count++] = &gpio_pin_configs[i].dev;
        }
    }
    if (type == BOARD_DEVICE_TYPE_ALL || type == BOARD_DEVICE_TYPE_I2C) {
        for (int i = 0; i < ARRAY_SIZE(i2c_configs) && count < size_devs; i++) {
            devs[count++] = &i2c_configs[i].dev;
        }
    }
    if (type == BOARD_DEVICE_TYPE_ALL || type == BOARD_DEVICE_TYPE_SPI) {
        for (int i = 0; i < ARRAY_SIZE(spi_configs) && count < size_devs; i++) {
            devs[count++] = &spi_configs[i].dev;
        }
    }
    return count;
}

const struct board_info *board_get_info(void) { return &board_info; }
