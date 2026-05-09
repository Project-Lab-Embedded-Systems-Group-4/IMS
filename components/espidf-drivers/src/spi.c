#include <driver/spi_master.h>
#include <string.h>

#include "espidf-drivers/spi.h"
#include "ims-mcu-driver/spi.h"

#define TAG "espidf_spi"

static int espidf_spi_configure(const struct ims_device *dev,
                                const struct ims_spi_config *config) {
    return -ENOTSUP;
}

static int espidf_spi_transfer(const struct ims_device *dev, const uint8_t *wbuf,
                               uint8_t *rbuf, size_t num_bytes) {
    spi_transaction_t trans;
    memset(&trans, 0, sizeof(trans));

    trans.length = num_bytes * 8;
    trans.tx_buffer = wbuf;
    trans.rx_buffer = rbuf;

    struct espidf_spidev_data *data = dev->data;
    esp_err_t esp_err = spi_device_acquire_bus(data->handle, portMAX_DELAY);
    if (esp_err != ESP_OK) {
        ESP_LOGE(TAG, "acquire bus: %s\n", esp_err_to_name(esp_err));
        return -EBUSY;
    }
    esp_err = spi_device_transmit(data->handle, &trans);
    spi_device_release_bus(data->handle);
    if (esp_err != ESP_OK) {
        return -EIO;
    }
    return 0;
}

static struct ims_spi_driver_api espidf_spi_api = {
    .configure = espidf_spi_configure,
    .transfer = espidf_spi_transfer,
};

esp_err_t espidf_spidev_init(struct ims_device *dev,
                             const struct espidf_spidev_config *config,
                             struct espidf_spidev_data *data) {
    dev->config = config;
    dev->data = data;
    dev->api = &espidf_spi_api;
    return spi_bus_add_device(config->port, &config->config, &data->handle);
}

esp_err_t espidf_spibus_init(const struct espidf_spibus_config *config) {
    return spi_bus_initialize(config->port, &config->config, config->dma_chan);
}
