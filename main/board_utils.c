#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>
#include "board/board.h"
#include "board_utils.h"
#include "ims-mcu-driver/gpio.h"

#define TAG "board_utils"

static struct board_utils_info g_board_info = {
    .subj_channel = -1,
    .zm_fb_index = -1,
    .rm_range_index = -1,
    .measure_enabled = false
};

static SemaphoreHandle_t g_resource_mutex = NULL;

esp_err_t board_utils_init(void) {
    if (g_resource_mutex == NULL) {
        g_resource_mutex = xSemaphoreCreateMutex();
    }
    return (g_resource_mutex != NULL) ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t board_utils_lock(uint32_t timeout_ms) {
    if (g_resource_mutex == NULL) {
        board_utils_init();
    }
    if (xSemaphoreTake(g_resource_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}

void board_utils_unlock(void) {
    if (g_resource_mutex != NULL) {
        xSemaphoreGive(g_resource_mutex);
    }
}

esp_err_t board_set_subj_channel(enum board_subj_channel channel) {
    if ((int)channel > 15) return ESP_ERR_INVALID_ARG;

    const char *pins[] = {"subj_a0", "subj_a1", "subj_a2", "subj_a3"};
    for (int i = 0; i < 4; i++) {
        const struct ims_device *dev = board_get_device(pins[i]);
        if (!dev) return ESP_ERR_NOT_FOUND;
        ims_gpio_pin_set(dev, ((int)channel >> i) & 0x01);
    }
    g_board_info.subj_channel = (int)channel;
    return ESP_OK;
}

esp_err_t board_set_zm_fb(uint8_t index) {
    if (index > 3) return ESP_ERR_INVALID_ARG;

    const char *pins[] = {"zm_fb_a0", "zm_fb_a1"};
    for (int i = 0; i < 2; i++) {
        const struct ims_device *dev = board_get_device(pins[i]);
        if (!dev) return ESP_ERR_NOT_FOUND;
        ims_gpio_pin_set(dev, (index >> i) & 0x01);
    }
    g_board_info.zm_fb_index = index;
    return ESP_OK;
}

esp_err_t board_set_rm_range(uint8_t index) {
    if (index > 3) return ESP_ERR_INVALID_ARG;

    const char *pins[] = {"rm_range_a0", "rm_range_a1"};
    for (int i = 0; i < 2; i++) {
        const struct ims_device *dev = board_get_device(pins[i]);
        if (!dev) return ESP_ERR_NOT_FOUND;
        ims_gpio_pin_set(dev, (index >> i) & 0x01);
    }
    g_board_info.rm_range_index = index;
    return ESP_OK;
}

esp_err_t board_measure_enable(bool enable) {
    const struct ims_device *dev = board_get_device("measure_en");
    if (!dev) return ESP_ERR_NOT_FOUND;
    esp_err_t err = ims_gpio_pin_set(dev, enable ? 1 : 0);
    if (err == ESP_OK) {
        g_board_info.measure_enabled = enable;
    }
    return err;
}

void board_utils_get_info(struct board_utils_info *info) {
    if (info) {
        memcpy(info, &g_board_info, sizeof(struct board_utils_info));
    }
}
