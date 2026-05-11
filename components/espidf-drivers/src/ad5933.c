#include <esp_log.h>
#include <esp_timer.h>
#include <math.h>
#include <string.h>

#include "ims-mcu-driver/assert.h"
#include "ims-mcu-driver/device.h"
#include "ims-mcu-driver/i2c.h"
#include "ims-mcu-driver/sensor/ad5933.h"
#include "ims-mcu-driver/util.h"

#define TAG "ad5933"

/* Internal Clock Speed */
#define AD5933_INTERNAL_CLOCK_FREQ 16776000
#define I2C_TIMEOUT_MS 100

/* Internal Helpers */

static int ad5933_write_raw_reg(const struct ims_device *dev, uint8_t reg,
                                uint8_t val) {
    const struct ad5933_config *cfg = dev->config;
    uint8_t data[2] = {reg, val};
    return ims_i2c_write(cfg->i2c_bus, cfg->i2c_addr, data, 2, I2C_TIMEOUT_MS);
}

static int ad5933_read_raw_reg(const struct ims_device *dev, uint8_t reg,
                               uint8_t *val) {
    const struct ad5933_config *cfg = dev->config;
    int ret =
        ims_i2c_write(cfg->i2c_bus, cfg->i2c_addr, &reg, 1, I2C_TIMEOUT_MS);
    if (ret != 0)
        return ret;
    return ims_i2c_read(cfg->i2c_bus, cfg->i2c_addr, val, 1, I2C_TIMEOUT_MS);
}

/* Register Access Functions */

int ad5933_get_ctrl_reg1(const struct ims_device *dev,
                         ad5933_ctrl_reg1_t *reg) {
    return ad5933_read_raw_reg(dev, AD5933_REG_CTRL_1, &reg->raw);
}

int ad5933_set_ctrl_reg1(const struct ims_device *dev, ad5933_ctrl_reg1_t reg) {
    struct ad5933_data *data = dev->data;
    int ret = ad5933_write_raw_reg(dev, AD5933_REG_CTRL_1, reg.raw);
    if (ret == 0)
        data->ctrl1 = reg;
    return ret;
}

int ad5933_get_ctrl_reg2(const struct ims_device *dev,
                         ad5933_ctrl_reg2_t *reg) {
    return ad5933_read_raw_reg(dev, AD5933_REG_CTRL_2, &reg->raw);
}

int ad5933_set_ctrl_reg2(const struct ims_device *dev, ad5933_ctrl_reg2_t reg) {
    struct ad5933_data *data = dev->data;
    int ret = ad5933_write_raw_reg(dev, AD5933_REG_CTRL_2, reg.raw);
    if (ret == 0)
        data->ctrl2 = reg;
    return ret;
}

int ad5933_get_status_reg(const struct ims_device *dev,
                          ad5933_status_reg_t *reg) {
    return ad5933_read_raw_reg(dev, AD5933_REG_STATUS, &reg->raw);
}

int ad5933_get_start_freq(const struct ims_device *dev, uint32_t *freq_hz) {
    struct ad5933_data *data = dev->data;
    uint8_t b1, b2, b3;
    ERROR_CHECK(ad5933_read_raw_reg(dev, AD5933_REG_START_FREQ_1, &b1));
    ERROR_CHECK(ad5933_read_raw_reg(dev, AD5933_REG_START_FREQ_2, &b2));
    ERROR_CHECK(ad5933_read_raw_reg(dev, AD5933_REG_START_FREQ_3, &b3));
    uint32_t val = (b1 << 16) | (b2 << 8) | b3;
    *freq_hz = (uint32_t)(((uint64_t)val * (data->clock_freq / 4)) >> 27);
    return 0;
}

int ad5933_get_inc_freq(const struct ims_device *dev, uint32_t *freq_hz) {
    struct ad5933_data *data = dev->data;
    uint8_t b1, b2, b3;
    ERROR_CHECK(ad5933_read_raw_reg(dev, AD5933_REG_INC_FREQ_1, &b1));
    ERROR_CHECK(ad5933_read_raw_reg(dev, AD5933_REG_INC_FREQ_2, &b2));
    ERROR_CHECK(ad5933_read_raw_reg(dev, AD5933_REG_INC_FREQ_3, &b3));
    uint32_t val = (b1 << 16) | (b2 << 8) | b3;
    *freq_hz = (uint32_t)(((uint64_t)val * (data->clock_freq / 4)) >> 27);
    return 0;
}

int ad5933_get_num_inc(const struct ims_device *dev, uint16_t *num) {
    uint8_t b1, b2;
    ERROR_CHECK(ad5933_read_raw_reg(dev, AD5933_REG_NUM_INC_1, &b1));
    ERROR_CHECK(ad5933_read_raw_reg(dev, AD5933_REG_NUM_INC_2, &b2));
    *num = ((b1 & 0x01) << 8) | b2;
    return 0;
}

int ad5933_get_settling_cycles(const struct ims_device *dev, uint16_t *cycles,
                               enum ad5933_settle_mul *mul) {
    ad5933_settle_reg1_t reg1;
    uint8_t b2;
    ERROR_CHECK(ad5933_read_raw_reg(dev, AD5933_REG_SETTLE_1, &reg1.raw));
    ERROR_CHECK(ad5933_read_raw_reg(dev, AD5933_REG_SETTLE_2, &b2));
    *cycles = (reg1.msb_cycles << 8) | b2;
    *mul = (enum ad5933_settle_mul)reg1.cycle_multiplier;
    return 0;
}

int ad5933_set_start_freq(const struct ims_device *dev, uint32_t freq_hz) {
    struct ad5933_data *data = dev->data;
    uint32_t val =
        (uint32_t)(((uint64_t)freq_hz << 27) / (data->clock_freq / 4));
    ERROR_CHECK(
        ad5933_write_raw_reg(dev, AD5933_REG_START_FREQ_1, (val >> 16) & 0xFF));
    ERROR_CHECK(
        ad5933_write_raw_reg(dev, AD5933_REG_START_FREQ_2, (val >> 8) & 0xFF));
    ERROR_CHECK(ad5933_write_raw_reg(dev, AD5933_REG_START_FREQ_3, val & 0xFF));
    data->start_freq = freq_hz;
    return 0;
}

int ad5933_set_inc_freq(const struct ims_device *dev, uint32_t freq_hz) {
    struct ad5933_data *data = dev->data;
    uint32_t val =
        (uint32_t)(((uint64_t)freq_hz << 27) / (data->clock_freq / 4));
    ERROR_CHECK(
        ad5933_write_raw_reg(dev, AD5933_REG_INC_FREQ_1, (val >> 16) & 0xFF));
    ERROR_CHECK(
        ad5933_write_raw_reg(dev, AD5933_REG_INC_FREQ_2, (val >> 8) & 0xFF));
    ERROR_CHECK(ad5933_write_raw_reg(dev, AD5933_REG_INC_FREQ_3, val & 0xFF));
    data->inc_freq = freq_hz;
    return 0;
}

int ad5933_set_num_inc(const struct ims_device *dev, uint16_t num) {
    struct ad5933_data *data = dev->data;
    ERROR_CHECK(
        ad5933_write_raw_reg(dev, AD5933_REG_NUM_INC_1, (num >> 8) & 0xFF));
    ERROR_CHECK(ad5933_write_raw_reg(dev, AD5933_REG_NUM_INC_2, num & 0xFF));
    data->num_inc = num;
    return 0;
}

int ad5933_set_settling_cycles(const struct ims_device *dev, uint16_t cycles,
                               enum ad5933_settle_mul mul) {
    struct ad5933_data *data = dev->data;
    ad5933_settle_reg1_t reg1 = {.raw = 0};
    reg1.msb_cycles = (cycles >> 8) & 0x01;
    reg1.cycle_multiplier = mul;

    ERROR_CHECK(ad5933_write_raw_reg(dev, AD5933_REG_SETTLE_1, reg1.raw));
    ERROR_CHECK(ad5933_write_raw_reg(dev, AD5933_REG_SETTLE_2, cycles & 0xFF));

    data->settling_cycles = cycles;
    data->settle_mul = mul;
    return 0;
}

int ad5933_set_pga_gain(const struct ims_device *dev,
                        enum ad5933_pga_gain gain) {
    ad5933_ctrl_reg1_t reg;
    ERROR_CHECK(ad5933_get_ctrl_reg1(dev, &reg));
    reg.pga_gain = gain;
    return ad5933_set_ctrl_reg1(dev, reg);
}

int ad5933_set_voltage_range(const struct ims_device *dev,
                             enum ad5933_voltage_range range) {
    ad5933_ctrl_reg1_t reg;
    ERROR_CHECK(ad5933_get_ctrl_reg1(dev, &reg));
    reg.output_voltage_range = range;
    return ad5933_set_ctrl_reg1(dev, reg);
}

/* Logic Implementation */

int ad5933_init(struct ims_device *dev, const struct ad5933_config *config,
                struct ad5933_data *data) {
    dev->config = config;
    dev->data = data;
    memset(data, 0, sizeof(*data));

    ERROR_CHECK(ad5933_reset(dev));

    ERROR_CHECK(ad5933_get_ctrl_reg1(dev, &data->ctrl1));
    ERROR_CHECK(ad5933_get_ctrl_reg2(dev, &data->ctrl2));

    if (config->ext_clock_freq > 0) {
        data->clock_freq = config->ext_clock_freq;
        data->ctrl2.clock_source = AD5933_CLK_EXT;
    } else {
        data->clock_freq = AD5933_INTERNAL_CLOCK_FREQ;
        data->ctrl2.clock_source = AD5933_CLK_INT;
    }

    /* Initialize to standby */
    data->ctrl1.function_code = AD5933_FUNC_STANDBY;
    ERROR_CHECK(ad5933_set_ctrl_reg1(dev, data->ctrl1));
    ERROR_CHECK(ad5933_set_ctrl_reg2(dev, data->ctrl2));

    return 0;
}

int ad5933_reset(const struct ims_device *dev) {
    struct ad5933_data *data = dev->data;
    ad5933_ctrl_reg2_t reg = data->ctrl2;
    reg.reset = 1;
    data->sweep_index = -1;
    return ad5933_set_ctrl_reg2(dev, reg);
}

int ad5933_start_sweep(const struct ims_device *dev) {
    struct ad5933_data *data = dev->data;

    /* 1. Standby */
    data->ctrl1.function_code = AD5933_FUNC_STANDBY;
    ERROR_CHECK(ad5933_set_ctrl_reg1(dev, data->ctrl1));

    /* 2. Init with start frequency */
    data->ctrl1.function_code = AD5933_FUNC_INIT_START_FREQ;
    ERROR_CHECK(ad5933_set_ctrl_reg1(dev, data->ctrl1));

    /* 3. Start frequency sweep */
    data->ctrl1.function_code = AD5933_FUNC_START_FREQ_SWEEP;
    ERROR_CHECK(ad5933_set_ctrl_reg1(dev, data->ctrl1));

    data->sweep_index = 0;

    uint32_t mul_factor = 1;
    if (data->settle_mul == AD5933_SETTLE_X2)
        mul_factor = 2;
    else if (data->settle_mul == AD5933_SETTLE_X4)
        mul_factor = 4;

    data->settling_time_us = (uint64_t)data->settling_cycles * mul_factor *
                             1000000 / data->start_freq;
    data->start_time_us = esp_timer_get_time();

    return 0;
}

int ad5933_advance_sweep(const struct ims_device *dev) {
    struct ad5933_data *data = dev->data;
    if (data->sweep_index >= data->num_inc)
        return 1; // Done

    data->sweep_index++;
    data->ctrl1.function_code = AD5933_FUNC_INCREMENT_FREQ;
    ERROR_CHECK(ad5933_set_ctrl_reg1(dev, data->ctrl1));
    data->start_time_us = esp_timer_get_time();

    return 0;
}

int ad5933_get_complex_data(const struct ims_device *dev, int16_t *real,
                            int16_t *imag, bool unsafe) {
    struct ad5933_data *data = dev->data;
    uint64_t now = esp_timer_get_time();
    uint64_t target =
        data->start_time_us + data->settling_time_us + (unsafe ? 500 : 540);

    if (now < target) {
        ims_msleep((target - now) / 1000 + 1);
    }

    if (!unsafe) {
        ad5933_status_reg_t status;
        do {
            ad5933_get_status_reg(dev, &status);
        } while (!status.data_valid);
    }

    uint8_t r1, r2, i1, i2;
    ERROR_CHECK(ad5933_read_raw_reg(dev, AD5933_REG_REAL_1, &r1));
    ERROR_CHECK(ad5933_read_raw_reg(dev, AD5933_REG_REAL_2, &r2));
    ERROR_CHECK(ad5933_read_raw_reg(dev, AD5933_REG_IMAG_1, &i1));
    ERROR_CHECK(ad5933_read_raw_reg(dev, AD5933_REG_IMAG_2, &i2));

    *real = (int16_t)((r1 << 8) | r2);
    *imag = (int16_t)((i1 << 8) | i2);

    return 0;
}

float ad5933_get_temperature(const struct ims_device *dev) {
    struct ad5933_data *data = dev->data;
    ad5933_ctrl_reg1_t original_ctrl = data->ctrl1;

    /* Start temp measure */
    ad5933_ctrl_reg1_t temp_ctrl = original_ctrl;
    temp_ctrl.function_code = AD5933_FUNC_MEASURE_TEMP;
    ad5933_set_ctrl_reg1(dev, temp_ctrl);

    ad5933_status_reg_t status;
    for (int i = 0; i < 100; i++) {
        ad5933_get_status_reg(dev, &status);
        if (status.temp_valid)
            break;
        ims_msleep(1);
    }

    if (!status.temp_valid)
        return -1.0f;

    uint8_t t1, t2;
    ad5933_read_raw_reg(dev, AD5933_REG_TEMP_1, &t1);
    ad5933_read_raw_reg(dev, AD5933_REG_TEMP_2, &t2);

    /* 14-bit 2's complement */
    int16_t temp_val = (int16_t)(((t1 & 0x3F) << 8) | t2);
    if (t1 & 0x20) { // Sign bit is D13 (5th bit of t1 if 14-bit)
        return (temp_val - 16384) / 32.0f;
    } else {
        return temp_val / 32.0f;
    }
}
