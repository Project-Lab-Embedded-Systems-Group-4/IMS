#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board_utils.h"
#include "event.h"
#include "ims-mcu-driver/sensor/ad5933.h"
#include "service.h"
#include "services/ad5933/ad5933_service.h"

#define TAG "ad5933_srv"

/* Calibration Defaults */
#define AD5933_CALI_CHANNEL BOARD_SUBJ_CH_CAL2
#define AD5933_CALI_RESISTOR 49900.0
#define AD5933_CALI_FB_INDEX 1

enum ad5933_service_state {
    AD5933_STATE_IDLE = 0,
    AD5933_STATE_CALIBRATE,       /* Startup Calibration */
    AD5933_STATE_STANDBY,         /* Step 1 */
    AD5933_STATE_INIT_START_FREQ, /* Step 2 */
    AD5933_STATE_SWEEPING,        /* Step 3 */
    AD5933_STATE_READ_DATA,       /* Data Valid Poll */
    AD5933_STATE_INCREMENTING,    /* Next Point */
    AD5933_STATE_POWER_DOWN,      /* Finish */
    AD5933_STATE_MAX
};

static const char *state_names[] = {
    "IDLE",     "CALIBRATE", "STANDBY",      "INIT_START_FREQ",
    "SWEEPING", "READ_DATA", "INCREMENTING", "POWER_DOWN"};

#define MAX_SAMPLES 512

struct ad5933_service_data {
    enum ad5933_service_state now_state;
    enum ad5933_service_state pre_state;
    uint64_t state_start_time_us;
    SemaphoreHandle_t start_sem;
    SemaphoreHandle_t data_mutex;

    struct ad5933_sample_data samples[MAX_SAMPLES];
    uint16_t sample_count;

    bool is_calibrating;
};

static struct service *g_ad5933_srv = NULL;

static void ad5933_event_handler(void *arg, esp_event_base_t base,
                                 int32_t event_id, void *event_data) {
    struct service *s = (struct service *)arg;
    struct ad5933_service_data *data = (struct ad5933_service_data *)s->data;

    if (base == IMS_EVENT_BASE) {
        switch (event_id) {
        case IMS_EVENT_AD5933_START_SWEEP:
            xSemaphoreGive(data->start_sem);
            break;
        case IMS_EVENT_AD5933_DATA_READY: {
            struct ad5933_service_config *cfg =
                (struct ad5933_service_config *)s->config;
            struct ad5933_data *drv_data =
                (struct ad5933_data *)cfg->ad5933_dev->data;
            double gf = drv_data->gain_factor;

            printf("Gain Factor: %e\n", gf);
            printf("%-6s | %-8s | %-8s | %-12s | %-15s\n", "Index", "Real",
                   "Imag", "Magnitude", "Impedance (Ohm)");
            printf("-------|----------|----------|--------------|----------------"
                   "\n");
            for (int i = 0; i < data->sample_count; i++) {
                double magnitude =
                    sqrt((double)data->samples[i].real * data->samples[i].real +
                         (double)data->samples[i].imag * data->samples[i].imag);
                double impedance = 0;
                if (gf != 0 && magnitude != 0) {
                    impedance = 1.0 / (gf * magnitude);
                }
                printf("%-6d | %-8d | %-8d | %-12.2f | %-15.2f\n", i,
                       data->samples[i].real, data->samples[i].imag, magnitude,
                       impedance);
            }
            break;
        }
        default:
            break;
        }
    }
}

static void ad5933_service_task(void *arg) {
    struct service *s = (struct service *)arg;
    struct ad5933_service_config *cfg =
        (struct ad5933_service_config *)s->config;
    struct ad5933_service_data *data = (struct ad5933_service_data *)s->data;
    const struct ims_device *ad_dev = cfg->ad5933_dev;

    ESP_LOGI(TAG, "AD5933 Service Task Started");

    while (service_need_stop(s) != ESP_OK) {
        ad5933_ctrl_reg1_t ctrl1;
        ad5933_status_reg_t status;
        uint32_t delay_ms = 0;

        if (data->now_state != data->pre_state) {
            ESP_LOGD(TAG, "State transition: %s -> %s",
                     state_names[data->pre_state],
                     state_names[data->now_state]);
            data->pre_state = data->now_state;
            data->state_start_time_us = esp_timer_get_time();
        }

        switch (data->now_state) {
        case AD5933_STATE_IDLE:
            xSemaphoreTake(data->start_sem, portMAX_DELAY);
            data->now_state = AD5933_STATE_STANDBY;
            data->is_calibrating = false;
            delay_ms = 0;
            break;

        case AD5933_STATE_CALIBRATE:
            ESP_LOGI(TAG, "Starting Auto-Calibration...");
            data->is_calibrating = true;
            board_set_subj_channel(AD5933_CALI_CHANNEL);
            board_set_zm_fb(AD5933_CALI_FB_INDEX);

            data->now_state = AD5933_STATE_STANDBY;
            delay_ms = 0;
            break;

        case AD5933_STATE_STANDBY:
            /* Lock shared board resources before starting measurement */
            if (board_utils_lock(portMAX_DELAY) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to lock board resources");
                data->now_state = AD5933_STATE_IDLE;
                break;
            }

            /* Step 1: Place AD5933 into standby mode */
            ad5933_get_ctrl_reg1(ad_dev, &ctrl1);
            ctrl1.function_code = AD5933_FUNC_STANDBY;
            ad5933_set_ctrl_reg1(ad_dev, ctrl1);

            xSemaphoreTake(data->data_mutex, portMAX_DELAY);
            data->sample_count = 0;
            xSemaphoreGive(data->data_mutex);

            board_measure_enable(true);

            data->now_state = AD5933_STATE_INIT_START_FREQ;
            delay_ms = 0;
            break;

        case AD5933_STATE_INIT_START_FREQ:
            /* Step 2: Program initialize with start frequency */
            ad5933_get_ctrl_reg1(ad_dev, &ctrl1);
            ctrl1.function_code = AD5933_FUNC_INIT_START_FREQ;
            ad5933_set_ctrl_reg1(ad_dev, ctrl1);

            // TODO: Add timeout handling here
            delay_ms = 1;
            data->now_state = AD5933_STATE_SWEEPING;
            break;

        case AD5933_STATE_SWEEPING:
            /* Step 3: Program start frequency sweep */
            ad5933_get_ctrl_reg1(ad_dev, &ctrl1);
            ctrl1.function_code = AD5933_FUNC_START_FREQ_SWEEP;
            ad5933_set_ctrl_reg1(ad_dev, ctrl1);

            struct ad5933_data *d = (struct ad5933_data *)ad_dev->data;
            d->sweep_index = 0;

            data->now_state = AD5933_STATE_READ_DATA;
            delay_ms = 0;
            break;

        case AD5933_STATE_READ_DATA:
            ad5933_get_status_reg(ad_dev, &status);
            if (status.data_valid) {
                int16_t real, imag;
                if (ad5933_get_complex_data(ad_dev, &real, &imag, true) == 0) {
                    if (data->is_calibrating) {
                        double mag =
                            sqrt((double)real * real + (double)imag * imag);
                        double gf = 1.0 / (mag * AD5933_CALI_RESISTOR);
                        ad5933_set_gain_factor(ad_dev, gf);
                        ESP_LOGI(TAG, "Calibration Gain Factor: %e", gf);
                    } else {
                        xSemaphoreTake(data->data_mutex, portMAX_DELAY);
                        if (data->sample_count < MAX_SAMPLES) {
                            data->samples[data->sample_count].real = real;
                            data->samples[data->sample_count].imag = imag;
                            data->sample_count++;
                        }
                        xSemaphoreGive(data->data_mutex);
                    }
                }

                if (status.sweep_complete || data->is_calibrating) {
                    data->now_state = AD5933_STATE_POWER_DOWN;
                } else {
                    data->now_state = AD5933_STATE_INCREMENTING;
                }
                delay_ms = 0;
            } else {
                delay_ms = 1;
            }
            break;

        case AD5933_STATE_INCREMENTING:
            ad5933_get_ctrl_reg1(ad_dev, &ctrl1);
            ctrl1.function_code = AD5933_FUNC_INCREMENT_FREQ;
            ad5933_set_ctrl_reg1(ad_dev, ctrl1);

            struct ad5933_data *d_inc = (struct ad5933_data *)ad_dev->data;
            d_inc->sweep_index++;

            data->now_state = AD5933_STATE_READ_DATA;
            delay_ms = 0;
            break;

        case AD5933_STATE_POWER_DOWN:
            ad5933_get_ctrl_reg1(ad_dev, &ctrl1);
            ctrl1.function_code = AD5933_FUNC_POWER_DOWN;
            ad5933_set_ctrl_reg1(ad_dev, ctrl1);

            board_measure_enable(false);
            board_utils_unlock();

            esp_event_post_to(cfg->loop, IMS_EVENT_BASE,
                              IMS_EVENT_AD5933_DATA_READY, NULL, 0,
                              portMAX_DELAY);

            data->now_state = AD5933_STATE_IDLE;
            data->is_calibrating = false;
            delay_ms = 0;
            break;

        default:
            data->now_state = AD5933_STATE_IDLE;
            delay_ms = 10;
            break;
        }

        if (delay_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
    }

    service_mark_done(s);
    vTaskDelete(NULL);
}

static esp_err_t ad5933_run(struct service *s) {
    struct ad5933_service_config *cfg =
        (struct ad5933_service_config *)s->config;

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(
        cfg->loop, IMS_EVENT_BASE, ESP_EVENT_ANY_ID,
        ad5933_event_handler, s, NULL));

    BaseType_t ret =
        xTaskCreate(ad5933_service_task, s->name, 4096, s, 5, &s->handle);
    return (ret == pdPASS) ? ESP_OK : ESP_FAIL;
}

static const struct service_api ad5933_api = {
    .run = ad5933_run,
};

esp_err_t ad5933_service_init(struct service *s,
                              struct ad5933_service_config *config) {
    struct ad5933_service_data *data =
        calloc(1, sizeof(struct ad5933_service_data));
    if (data == NULL)
        return ESP_ERR_NO_MEM;

    data->start_sem = xSemaphoreCreateBinary();
    data->data_mutex = xSemaphoreCreateMutex();
    data->now_state = AD5933_STATE_CALIBRATE;
    data->pre_state = AD5933_STATE_MAX;

    s->api = &ad5933_api;
    s->config = config;
    s->data = data;
    s->name = "ad5933_srv";

    g_ad5933_srv = s;

    return service_run(s);
}

esp_err_t ad5933_service_get_results(struct ad5933_sample_data **out_samples,
                                     uint16_t *out_count) {
    if (g_ad5933_srv == NULL)
        return ESP_ERR_INVALID_STATE;
    struct ad5933_service_data *data =
        (struct ad5933_service_data *)g_ad5933_srv->data;

    if (data->now_state != AD5933_STATE_IDLE) {
        return ESP_ERR_INVALID_STATE; // Sweep in progress
    }

    *out_samples = data->samples;
    *out_count = data->sample_count;
    return ESP_OK;
}
