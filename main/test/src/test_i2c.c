#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "espidf-drivers/i2c.h"
#include "ims-mcu-driver/i2c.h"
#include "test_i2c.h"

#define I2C_MASTER_SCL_IO           22
#define I2C_MASTER_SDA_IO           21
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000

#define I2C_SLAVE_SCL_IO            19
#define I2C_SLAVE_SDA_IO            18
#define I2C_SLAVE_NUM               I2C_NUM_1
#define I2C_SLAVE_TX_BUF_LEN        256
#define I2C_SLAVE_RX_BUF_LEN        256
#define I2C_SLAVE_ADDR              0x28

void test_i2c_run(void)
{
    printf("Starting I2C Master-Slave Test...\n");

    // 1. Initialize Slave (I2C1) using standard ESP-IDF
    i2c_config_t slave_conf = {
        .mode = I2C_MODE_SLAVE,
        .sda_io_num = I2C_SLAVE_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_SLAVE_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .slave.addr_10bit_en = 0,
        .slave.slave_addr = I2C_SLAVE_ADDR,
    };
    esp_err_t err = i2c_param_config(I2C_SLAVE_NUM, &slave_conf);
    if (err != ESP_OK) printf("Slave param config failed\n");
    err = i2c_driver_install(I2C_SLAVE_NUM, slave_conf.mode, I2C_SLAVE_RX_BUF_LEN, I2C_SLAVE_TX_BUF_LEN, 0);
    if (err != ESP_OK) printf("Slave driver install failed\n");

    // 2. Initialize Master (I2C0) using IMS driver
    static struct ims_device master_dev = { .name = "i2c-master" };
    static struct espidf_i2c_config master_cfg = {
        .port = I2C_MASTER_NUM,
        .config = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = I2C_MASTER_SDA_IO,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_io_num = I2C_MASTER_SCL_IO,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = I2C_MASTER_FREQ_HZ,
        },
        .rxbuf_size = 0,
        .txbuf_size = 0,
    };
    static struct espidf_i2c_data master_data; // static zero init

    err = espidf_i2c_init(&master_dev, &master_cfg, &master_data);
    if (err != ESP_OK) {
        printf("Failed to initialize I2C master driver: %d\n", err);
        return;
    }
    printf("I2C Master initialized\n");

    // 3. Test Transfer
    const char *send_data = "Hello I2C Slave!";
    uint8_t recv_data[32] = { 0 };

    printf("Master sending: %s\n", send_data);
    // Use the abstraction API
    err = ims_i2c_write(&master_dev, I2C_SLAVE_ADDR, (uint8_t *)send_data, strlen(send_data), 1000);
    
    if (err != 0) {
        printf("Master write failed: %d\n", err);
    } else {
        int len = i2c_slave_read_buffer(I2C_SLAVE_NUM, recv_data, sizeof(recv_data), 1000 / portTICK_PERIOD_MS);
        if (len > 0) {
            printf("Slave received %d bytes: %s\n", len, (char *)recv_data);
        } else {
            printf("Slave read timeout or no data\n");
        }
    }
}
