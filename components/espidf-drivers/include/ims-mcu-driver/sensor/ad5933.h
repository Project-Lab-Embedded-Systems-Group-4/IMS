#ifndef IMS_MCU_DRIVER_SENSOR_AD_AD5933_H_
#define IMS_MCU_DRIVER_SENSOR_AD_AD5933_H_

#include "ims-mcu-driver/device.h"
#include <stdbool.h>
#include <stdint.h>

#define AD5933_I2C_ADDR 0x0D

/* Register Addresses */
#define AD5933_REG_CTRL_1 0x80
#define AD5933_REG_CTRL_2 0x81
#define AD5933_REG_START_FREQ_1 0x82
#define AD5933_REG_START_FREQ_2 0x83
#define AD5933_REG_START_FREQ_3 0x84
#define AD5933_REG_INC_FREQ_1 0x85
#define AD5933_REG_INC_FREQ_2 0x86
#define AD5933_REG_INC_FREQ_3 0x87
#define AD5933_REG_NUM_INC_1 0x88
#define AD5933_REG_NUM_INC_2 0x89
#define AD5933_REG_SETTLE_1 0x8A
#define AD5933_REG_SETTLE_2 0x8B
#define AD5933_REG_STATUS 0x8F
#define AD5933_REG_TEMP_1 0x92
#define AD5933_REG_TEMP_2 0x93
#define AD5933_REG_REAL_1 0x94
#define AD5933_REG_REAL_2 0x95
#define AD5933_REG_IMAG_1 0x96
#define AD5933_REG_IMAG_2 0x97

/* Bitfield Structs */

/**
 * @brief Control Register 1 (0x80)
 */
typedef union {
    struct {
        uint8_t pga_gain : 1;             /* D8: 0 = x5, 1 = x1 */
        uint8_t output_voltage_range : 2; /* D10-D9: 00=2.0V, 01=200mV,
                                             10=400mV, 11=1.0V */
        uint8_t reserved : 1;             /* D11: set to 0 */
        uint8_t function_code : 4;        /* D15-D12: Command code */
    };
    uint8_t raw;
} ad5933_ctrl_reg1_t;

/**
 * @brief Control Register 2 (0x81)
 */
typedef union {
    struct {
        uint8_t reserved1 : 3;    /* D2-D0: set to 0 */
        uint8_t clock_source : 1; /* D3: 0 = internal, 1 = external */
        uint8_t reset : 1;        /* D4: 1 = reset */
        uint8_t reserved2 : 3;    /* D7-D5: set to 0 */
    };
    uint8_t raw;
} ad5933_ctrl_reg2_t;

/**
 * @brief Status Register (0x8F)
 */
typedef union {
    struct {
        uint8_t temp_valid : 1;     /* D0: 1 = Valid temperature measurement */
        uint8_t data_valid : 1;     /* D1: 1 = Valid real/imaginary data */
        uint8_t sweep_complete : 1; /* D2: 1 = Frequency sweep complete */
        uint8_t reserved : 5;       /* D7-D3 */
    };
    uint8_t raw;
} ad5933_status_reg_t;

/**
 * @brief Number of Settling Time Cycles Register 1 (0x8A)
 */
typedef union {
    struct {
        uint8_t msb_cycles : 1; /* D8: MSB number of settling time cycles */
        uint8_t cycle_multiplier : 2; /* D10-D9: 00=x1, 01=x2, 11=x4 */
        uint8_t reserved : 5;         /* D15-D11: set to 0 */
    };
    uint8_t raw;
} ad5933_settle_reg1_t;

/* Enums for better readability */
enum ad5933_function {
    AD5933_FUNC_NO_OP = 0x0,
    AD5933_FUNC_INIT_START_FREQ = 0x1,
    AD5933_FUNC_START_FREQ_SWEEP = 0x2,
    AD5933_FUNC_INCREMENT_FREQ = 0x3,
    AD5933_FUNC_REPEAT_FREQ = 0x4,
    AD5933_FUNC_MEASURE_TEMP = 0x9,
    AD5933_FUNC_POWER_DOWN = 0xA,
    AD5933_FUNC_STANDBY = 0xB,
};

enum ad5933_pga_gain {
    AD5933_PGA_GAIN_X5 = 0,
    AD5933_PGA_GAIN_X1 = 1,
};

enum ad5933_voltage_range {
    AD5933_RANGE_2V_PP = 0,
    AD5933_RANGE_200MV_PP = 1,
    AD5933_RANGE_400MV_PP = 2,
    AD5933_RANGE_1V_PP = 3,
};

enum ad5933_clock_src {
    AD5933_CLK_INT = 0,
    AD5933_CLK_EXT = 1,
};

enum ad5933_settle_mul {
    AD5933_SETTLE_X1 = 0,
    AD5933_SETTLE_X2 = 1,
    AD5933_SETTLE_X4 = 3,
};

/* Device Config & Data */
struct ad5933_config {
    const struct ims_device *i2c_bus;
    uint16_t i2c_addr;
    uint32_t ext_clock_freq;
};

struct ad5933_data {
    uint32_t clock_freq;
    ad5933_ctrl_reg1_t ctrl1;
    ad5933_ctrl_reg2_t ctrl2;
    uint32_t start_freq;
    uint32_t inc_freq;
    uint16_t num_inc;
    uint16_t settling_cycles;
    enum ad5933_settle_mul settle_mul;
    int sweep_index;
    uint64_t start_time_us;
    uint64_t settling_time_us;
};

/* API Functions */
int ad5933_init(struct ims_device *dev, const struct ad5933_config *config,
                struct ad5933_data *data);

/* Register Access */
int ad5933_get_ctrl_reg1(const struct ims_device *dev, ad5933_ctrl_reg1_t *reg);
int ad5933_set_ctrl_reg1(const struct ims_device *dev, ad5933_ctrl_reg1_t reg);
int ad5933_get_ctrl_reg2(const struct ims_device *dev, ad5933_ctrl_reg2_t *reg);
int ad5933_set_ctrl_reg2(const struct ims_device *dev, ad5933_ctrl_reg2_t reg);
int ad5933_get_status_reg(const struct ims_device *dev,
                          ad5933_status_reg_t *reg);

int ad5933_set_start_freq(const struct ims_device *dev, uint32_t freq_hz);
int ad5933_set_inc_freq(const struct ims_device *dev, uint32_t freq_hz);
int ad5933_set_num_inc(const struct ims_device *dev, uint16_t num);
int ad5933_set_settling_cycles(const struct ims_device *dev, uint16_t cycles,
                               enum ad5933_settle_mul mul);

/* High-level logic */
int ad5933_reset(const struct ims_device *dev);
int ad5933_start_sweep(const struct ims_device *dev);
int ad5933_advance_sweep(const struct ims_device *dev);
int ad5933_get_complex_data(const struct ims_device *dev, int16_t *real,
                            int16_t *imag, bool unsafe);
float ad5933_get_temperature(const struct ims_device *dev);

#endif // IMS_MCU_DRIVER_SENSOR_AD_AD5933_H_
