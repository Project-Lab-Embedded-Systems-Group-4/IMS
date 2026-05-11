#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <stdlib.h>

#include "board_utils.h"
#include "event.h"
#include "services/ad7680/ad7680_service.h"

#define TAG "ad7680_srv"

struct ad7680_service_data {
    uint16_t last_value;
    uint16_t iterations;
    SemaphoreHandle_t data_mutex;
    SemaphoreHandle_t start_sem;
};

#define DEFAULT_RM_RANGE BOARD_RM_RANGE_2
#define DEFAULT_RM_REF_RESISTOR RM_REF_RES_VALUES[DEFAULT_RM_RANGE]
#define DEFAULT_RM_CAL_CHANNEL BOARD_SUBJ_CH_CAL1

static struct service *g_ad7680_srv = NULL;

static double calculate_resistance(uint16_t adc_val, uint32_t r_ref) {
    if (adc_val >= 65535) {
        return -1.0; // Over range
    }
    return (double)r_ref * (double)adc_val / (65536.0 - (double)adc_val);
}

static void ad7680_event_handler(void *arg, esp_event_base_t base,
                                 int32_t event_id, void *event_data) {
    struct service *s = (struct service *)arg;
    struct ad7680_service_data *data = (struct ad7680_service_data *)s->data;

    if (base == IMS_EVENT_BASE) {
        switch (event_id) {
        case IMS_EVENT_AD7680_START_READ:
            if (event_data != NULL) {
                data->iterations = *(uint16_t *)event_data;
            } else {
                data->iterations = 1;
            }
            xSemaphoreGive(data->start_sem);
            break;
        case IMS_EVENT_AD7680_DATA_READY: {
            uint16_t val = *(uint16_t *)event_data;
            printf("AD7680 Raw Value: %u\n", val);

            // Calculate Resistance
            struct board_utils_info info;
            board_utils_get_info(&info);
            if (info.rm_range_index >= 0 && info.rm_range_index < 4) {
                const uint32_t ref_res[] = RM_REF_RES_VALUES;
                uint32_t r_ref = ref_res[info.rm_range_index];
                double resistance = calculate_resistance(val, r_ref);

                if (resistance >= 0) {
                    printf("Measured Resistance: %.2f Ohm (Range Ref: %" PRIu32
                           " Ohm)\n",
                           resistance, r_ref);
                } else {
                    printf("Measured Resistance: Over Range\n");
                }
            } else {
                printf("Measured Resistance: Unknown Range\n");
            }
            break;
        }
        default:
            break;
        }
    }
}

static void ad7680_service_task(void *arg) {
    struct service *s = (struct service *)arg;
    struct ad7680_service_config *cfg =
        (struct ad7680_service_config *)s->config;
    struct ad7680_service_data *data = (struct ad7680_service_data *)s->data;

    ESP_LOGI(TAG, "AD7680 Service Task Started");

    const uint32_t ref_res[] = RM_REF_RES_VALUES;
    const uint32_t cal_res[] = RM_CAL_RES_VALUES;
    struct {
        enum board_subj_channel ch;
        enum board_rm_range range;
        uint32_t expected;
    } cal_points[] = {
        {BOARD_SUBJ_CH_CAL1, BOARD_RM_RANGE_4, cal_res[0]},
        {BOARD_SUBJ_CH_CAL2, BOARD_RM_RANGE_2, cal_res[1]},
        {BOARD_SUBJ_CH_CAL3, BOARD_RM_RANGE_1, cal_res[2]},
    };

    /* Startup Calibration */
    ESP_LOGI(TAG, "Starting Startup Calibration...");
    if (board_utils_lock(portMAX_DELAY) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to lock board resources for calibration");
        return;
    }

    board_measure_enable(true);
    for (int i = 0; i < 3; i++) {
        board_set_subj_channel(cal_points[i].ch);
        board_set_rm_range(cal_points[i].range);
        vTaskDelay(pdMS_TO_TICKS(50)); // Settle time
        uint16_t val;
        if (ad7680_read_averaging(cfg->ad7680_dev, &val, 64) == 0) {
            uint32_t r_ref = ref_res[cal_points[i].range];
            double resistance = calculate_resistance(val, r_ref);
            if (resistance >= 0) {
                ESP_LOGI(TAG,
                         "CAL%d: Expected %" PRIu32
                         " Ohm, Measured %.2f Ohm (Error: %.2f%%)",
                         i + 1, cal_points[i].expected, resistance,
                         (resistance - cal_points[i].expected) * 100.0 /
                             cal_points[i].expected);
            } else {
                ESP_LOGE(TAG, "CAL%d: Over Range!", i + 1);
            }
        } else {
            ESP_LOGE(TAG, "CAL%d: Read Failed!", i + 1);
        }
    }
    board_measure_enable(false);
    board_utils_unlock();
    ESP_LOGI(TAG, "Startup Calibration Complete");

    while (service_need_stop(s) != ESP_OK) {
        if (xSemaphoreTake(data->start_sem, portMAX_DELAY) == pdTRUE) {
            uint16_t val;

            if (board_utils_lock(portMAX_DELAY) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to lock board resources for measurement");
                continue;
            }

            board_measure_enable(true);
            vTaskDelay(pdMS_TO_TICKS(10)); // Settle
            if (ad7680_read_averaging(cfg->ad7680_dev, &val,
                                      data->iterations) == 0) {
                xSemaphoreTake(data->data_mutex, portMAX_DELAY);
                data->last_value = val;
                xSemaphoreGive(data->data_mutex);

                esp_event_post_to(cfg->loop, IMS_EVENT_BASE,
                                  IMS_EVENT_AD7680_DATA_READY, &val,
                                  sizeof(val), portMAX_DELAY);
            } else {
                ESP_LOGW(TAG, "Failed to read AD7680");
            }
            board_measure_enable(false);
            board_utils_unlock();
        }
    }

    service_mark_done(s);
    vTaskDelete(NULL);
}

static esp_err_t ad7680_run(struct service *s) {
    struct ad7680_service_config *cfg =
        (struct ad7680_service_config *)s->config;

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(
        cfg->loop, IMS_EVENT_BASE, IMS_EVENT_AD7680_START_READ,
        ad7680_event_handler, s, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(
        cfg->loop, IMS_EVENT_BASE, IMS_EVENT_AD7680_DATA_READY,
        ad7680_event_handler, s, NULL));

    BaseType_t ret =
        xTaskCreate(ad7680_service_task, s->name, 4096, s, 5, &s->handle);
    return (ret == pdPASS) ? ESP_OK : ESP_FAIL;
}

static const struct service_api ad7680_api = {
    .run = ad7680_run,
};

esp_err_t ad7680_service_init(struct service *s,
                              struct ad7680_service_config *config) {
    struct ad7680_service_data *data =
        calloc(1, sizeof(struct ad7680_service_data));
    if (data == NULL) {
        return ESP_ERR_NO_MEM;
    }

    data->data_mutex = xSemaphoreCreateMutex();
    data->start_sem = xSemaphoreCreateBinary();
    if (data->data_mutex == NULL || data->start_sem == NULL) {
        if (data->data_mutex)
            vSemaphoreDelete(data->data_mutex);
        if (data->start_sem)
            vSemaphoreDelete(data->start_sem);
        free(data);
        return ESP_ERR_NO_MEM;
    }

    s->api = &ad7680_api;
    s->config = config;
    s->data = data;
    s->name = "ad7680_srv";

    g_ad7680_srv = s;

    return service_run(s);
}

esp_err_t ad7680_service_get_value(uint16_t *out_value) {
    if (g_ad7680_srv == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    struct ad7680_service_data *data =
        (struct ad7680_service_data *)g_ad7680_srv->data;

    xSemaphoreTake(data->data_mutex, portMAX_DELAY);
    *out_value = data->last_value;
    xSemaphoreGive(data->data_mutex);

    return ESP_OK;
}
