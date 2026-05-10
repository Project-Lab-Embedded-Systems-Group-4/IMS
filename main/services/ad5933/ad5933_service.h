#ifndef IMS_SERVICES_AD5933_SERVICE_H_
#define IMS_SERVICES_AD5933_SERVICE_H_

#include <esp_event.h>
#include "service.h"
#include "ims-mcu-driver/sensor/ad5933.h"

struct ad5933_service_config {
    esp_event_loop_handle_t loop;
    const struct ims_device *ad5933_dev;
};

struct ad5933_sample_data {
    int16_t real;
    int16_t imag;
};

esp_err_t ad5933_service_init(struct service *s, struct ad5933_service_config *config);

/**
 * @brief Get the last sweep results
 * 
 * @param out_samples Pointer to receive the buffer address
 * @param out_count Pointer to receive the number of samples
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if a sweep is in progress
 */
esp_err_t ad5933_service_get_results(struct ad5933_sample_data **out_samples, uint16_t *out_count);

#endif // IMS_SERVICES_AD5933_SERVICE_H_
