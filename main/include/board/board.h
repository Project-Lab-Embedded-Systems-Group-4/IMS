#ifndef IMS_BOARD_BOARD_H_
#define IMS_BOARD_BOARD_H_

#include <esp_err.h>
#include <stdint.h>

#include "ims-mcu-driver/device.h"

enum board_device_type {
    BOARD_DEVICE_TYPE_ALL = 0,
    BOARD_DEVICE_TYPE_GPIO_PORT,
    BOARD_DEVICE_TYPE_GPIO_PIN,
    BOARD_DEVICE_TYPE_I2C,
    BOARD_DEVICE_TYPE_SPI,
    BOARD_DEVICE_TYPE_ADC,
};

struct board_info {
    char name[32];
    uint16_t version;
    uint16_t revision;
};

esp_err_t board_init(void);
const struct ims_device *board_get_device(const char *name);
int board_get_device_by_type(enum board_device_type type,
                             const struct ims_device *devs[], int size_devs);
const struct board_info *board_get_info(void);

#endif // IMS_BOARD_BOARD_H_
