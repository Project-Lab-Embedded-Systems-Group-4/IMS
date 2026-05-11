#ifndef IMS_SERVICES_AD7680_SERVICE_H_
#define IMS_SERVICES_AD7680_SERVICE_H_

#include <esp_event.h>
#include "service.h"
#include "ims-mcu-driver/sensor/ad7680.h"

struct ad7680_service_config {
    esp_event_loop_handle_t loop;
    const struct ims_device *ad7680_dev;
};

esp_err_t ad7680_service_init(struct service *s, struct ad7680_service_config *config);

/**
 * @brief Get the last ADC reading
 * 
 * @param out_value Pointer to receive the ADC value
 * @return ESP_OK on success
 */
esp_err_t ad7680_service_get_value(uint16_t *out_value);

#endif // IMS_SERVICES_AD7680_SERVICE_H_
