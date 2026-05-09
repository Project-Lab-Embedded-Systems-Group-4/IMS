#ifndef ESP_IMS_MCU_DRIVER_ESPIDF_DRIVERS_SPI_H_
#define ESP_IMS_MCU_DRIVER_ESPIDF_DRIVERS_SPI_H_

#include <driver/spi_master.h>
#include <esp_err.h>
#include <hal/spi_types.h>

#include "ims-mcu-driver/spi.h"

struct espidf_spibus_config {
    spi_host_device_t port;
    spi_bus_config_t config;
    int dma_chan;
};

struct espidf_spidev_config {
    spi_host_device_t port;
    spi_device_interface_config_t config;
};

struct espidf_spidev_data {
    spi_device_handle_t handle;
};

esp_err_t espidf_spidev_init(struct ims_device *dev,
                             const struct espidf_spidev_config *config,
                             struct espidf_spidev_data *data);

esp_err_t espidf_spibus_init(const struct espidf_spibus_config *config);

#endif // ESP_IMS_MCU_DRIVER_ESPIDF_DRIVERS_SPI_H_
