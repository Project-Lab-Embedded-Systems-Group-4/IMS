#ifndef IMS_MCU_DRIVER_DEVICE_H_
#define IMS_MCU_DRIVER_DEVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/**
 * @brief Generic device structure per driver instance
 */
struct ims_device {
    // Name of device instance
    const char *name;
    // Address of the API exposed by the device instance
    const void *api;
    // Address of the device instance of configuration, usually fixed.
    const void *config;
    // Address of the device instance of data, you can put state or cache data
    // here.
    void *data;
};

#ifdef __cplusplus
}
#endif

#endif // IMS_MCU_DRIVER_DEVICE_H_
