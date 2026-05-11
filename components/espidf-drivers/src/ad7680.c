#include <string.h>
#include <esp_log.h>

#include "ims-mcu-driver/sensor/ad7680.h"
#include "ims-mcu-driver/spi.h"

#define TAG "ad7680"

int ad7680_init(struct ims_device *dev, const struct ad7680_config *config,
                struct ad7680_data *data) {
    dev->config = config;
    dev->data = data;
    ESP_LOGI(TAG, "AD7680 initialized");
    return 0;
}

int ad7680_read(const struct ims_device *dev, uint16_t *data) {
    return ad7680_read_averaging(dev, data, 1);
}

int ad7680_read_averaging(const struct ims_device *dev, uint16_t *data, int iterations) {
    const struct ad7680_config *cfg = dev->config;
    uint32_t sum = 0;
    uint8_t rbuf[3];
    int ret;

    if (iterations <= 0) {
        return -1;
    }

    for (int i = 0; i < iterations; i++) {
        ret = ims_spi_transfer(cfg->spi_bus, NULL, rbuf, 3);
        if (ret != 0) {
            ESP_LOGE(TAG, "SPI transfer failed: %d", ret);
            return ret;
        }

        /* 
         * AD7680 is a 16-bit ADC. 
         * Output data format: 4 leading zeros + 16 bits data + 4 trailing zeros (total 24 bits)
         * rbuf[0]: 0 0 0 0 D15 D14 D13 D12
         * rbuf[1]: D11 D10 D9 D8 D7 D6 D5 D4
         * rbuf[2]: D3 D2 D1 D0 0 0 0 0
         */
        uint16_t val = (((uint16_t)(rbuf[0] & 0x0F)) << 12) |
                       (((uint16_t)rbuf[1]) << 4) |
                       ((uint16_t)(rbuf[2] >> 4));
        sum += val;
    }

    *data = (uint16_t)(sum / iterations);
    return 0;
}
