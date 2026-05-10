#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "service.h"

#define TAG "services"

esp_err_t service_run(struct service *s) {
    s->stop_sem = xSemaphoreCreateBinary();
    s->done_sem = xSemaphoreCreateBinary();
    esp_err_t err = s->api->run(s);
    if (err != ESP_OK) {
        return err;
    }
    return ESP_OK;
}

esp_err_t service_stop(struct service *s) {
    if (xSemaphoreGive(s->stop_sem) != pdPASS) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t service_join(struct service *s, uint32_t timeout_ms) {
    if (xSemaphoreTake(s->done_sem, timeout_ms / portTICK_PERIOD_MS) !=
        pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t service_need_stop(struct service *s) {
    if (xSemaphoreTake(s->stop_sem, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t service_mark_done(struct service *s) {
    if (xSemaphoreGive(s->done_sem) != pdPASS) {
        return ESP_FAIL;
    }
    return ESP_OK;
}
