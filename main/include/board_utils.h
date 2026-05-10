#ifndef IMS_BOARD_UTILS_H_
#define IMS_BOARD_UTILS_H_

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Impedance Measurement (AD5933) Constants
 */
#define ZM_FB_RES_VALUES   { 0, 12000, 102000, 332000 }
#define ZM_CAL_RES_VALUES  { 4700, 49900, 330000 }

/**
 * @brief Resistance Measurement (AD7680) Constants
 */
#define RM_REF_RES_VALUES  { 330000, 33000, 3300, 330 }
#define RM_CAL_RES_VALUES  { 1000, 10000, 100000 }

/**
 * @brief Channel options for ZM_SUBJ and R_SUBJ multiplexers
 */
enum board_subj_channel {
    BOARD_SUBJ_CH1 = 0,
    BOARD_SUBJ_CH2 = 1,
    BOARD_SUBJ_CH3 = 2,
    BOARD_SUBJ_CH4 = 3,
    BOARD_SUBJ_CH5 = 4,
    BOARD_SUBJ_CH6 = 5,
    BOARD_SUBJ_CH7 = 6,
    BOARD_SUBJ_CH8 = 7,
    BOARD_SUBJ_CH9 = 8,
    BOARD_SUBJ_CH10 = 9,
    BOARD_SUBJ_CH_CAL1 = 10, /* ZM: 4.7k, RM: 1k */
    BOARD_SUBJ_CH_CAL2 = 12, /* ZM: 49.9k, RM: 10k */
    BOARD_SUBJ_CH_CAL3 = 14, /* ZM: 330k, RM: 100k */
};

/**
 * @brief Options for ZM_FB multiplexer feedback resistor selection
 */
enum board_zm_fb_source {
    BOARD_ZM_FB_1 = 0, /* 2k */
    BOARD_ZM_FB_2 = 1, /* 10k */
    BOARD_ZM_FB_3 = 2, /* 100k */
    BOARD_ZM_FB_4 = 3, /* 330k */
};

/**
 * @brief Options for R_RANGE multiplexer reference resistor selection
 */
enum board_rm_range {
    BOARD_RM_RANGE_1 = 0, /* 330k */
    BOARD_RM_RANGE_2 = 1, /* 33k */
    BOARD_RM_RANGE_3 = 2, /* 3.3k */
    BOARD_RM_RANGE_4 = 3, /* 330 */
};

/**
 * @brief Current board utility state
 */
struct board_utils_info {
    int subj_channel;      /* 0-15 */
    int zm_fb_index;       /* 0-3 */
    int rm_range_index;    /* 0-3 */
    bool measure_enabled;
};

/**
 * @brief Select the channel for ZM_SUBJ and R_SUBJ multiplexers (A0-A3)
 */
esp_err_t board_set_subj_channel(enum board_subj_channel channel);

/**
 * @brief Select the feedback resistor for ZM_FB multiplexer (A0-A1)
 */
esp_err_t board_set_zm_fb(uint8_t index);

/**
 * @brief Select the reference resistor for R_RANGE multiplexer (A0-A1)
 */
esp_err_t board_set_rm_range(uint8_t index);

/**
 * @brief Enable or disable the measurement circuits
 */
esp_err_t board_measure_enable(bool enable);

/**
 * @brief Get current configuration of board utilities
 */
void board_utils_get_info(struct board_utils_info *info);

#endif // IMS_BOARD_UTILS_H_
