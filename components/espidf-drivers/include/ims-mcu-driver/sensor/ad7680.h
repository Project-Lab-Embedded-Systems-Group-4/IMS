#ifndef IMS_MCU_DRIVER_SENSOR_AD_AD7680_H_
#define IMS_MCU_DRIVER_SENSOR_AD_AD7680_H_

#include "ims-mcu-driver/device.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ad7680_config {
    const struct ims_device *spi_bus;
};

struct ad7680_data {
    // No state needed for now
};

int ad7680_init(struct ims_device *dev, const struct ad7680_config *config,
                struct ad7680_data *data);

int ad7680_read(const struct ims_device *dev, uint16_t *data);
int ad7680_read_averaging(const struct ims_device *dev, uint16_t *data, int iterations);

#ifdef __cplusplus
}
#endif

#endif // IMS_MCU_DRIVER_SENSOR_AD_AD7680_H_
