#ifndef ESP_IMS_MCU_DRIVER_ESPIDF_DRIVERS_I2C_H_
#define ESP_IMS_MCU_DRIVER_ESPIDF_DRIVERS_I2C_H_

#include <driver/i2c.h>
#include <esp_err.h>

#include "os_port.h"
#include "ims-mcu-driver/i2c.h"

struct espidf_i2c_config {
    i2c_port_t port;
    i2c_config_t config;
    uint32_t rxbuf_size;
    uint32_t txbuf_size;
    uint32_t clkstretch_timeout_tick; // When APB clock 80MHz,
                                      // 0xFFFFF/80MHz= 13.107ms is the max
                                      // clock stretch timeout
};

struct espidf_i2c_data {
    ims_mutex_t mutex;
};

esp_err_t espidf_i2c_init(struct ims_device *dev,
                          const struct espidf_i2c_config *config,
                          struct espidf_i2c_data *data);

#endif // ESP_IMS_MCU_DRIVER_ESPIDF_DRIVERS_I2C_H_
