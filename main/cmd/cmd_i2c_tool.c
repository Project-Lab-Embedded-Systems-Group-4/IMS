#include <argtable3/argtable3.h>
#include <driver/i2c.h>
#include <esp_console.h>
#include <stdio.h>
#include <string.h>

#include "board/board.h"
#include "cmd.h"
#include "ims-mcu-driver/i2c.h"
#include "ims-mcu-driver/util.h"

static const char *TAG = "cmd_i2c";

#define SET_REG_TIMEOUT_MS 50
#define GET_REG_TIMEOUT_MS 50

static struct {
    struct arg_str *name;
    struct arg_str *chip_address;
    struct arg_str *register_address;
    struct arg_int *data_length;
    struct arg_end *end;
} i2cget_args;

static int do_i2cget_cmd(int argc, char **argv) {
    int nerrors = arg_parse(argc, argv, (void **)&i2cget_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, i2cget_args.end, argv[0]);
        return 0;
    }

    const struct ims_device *devs = {NULL};

    if (i2cget_args.name->count == 1) {
        devs = board_get_device(i2cget_args.name->sval[0]);
        if (devs == NULL) {
            return ESP_ERR_NOT_FOUND;
        }
    } else {
        return ESP_ERR_NOT_FOUND;
    }

    /* Check chip address: "-c" option */
    uint8_t chip_addr = 0;
    if (i2cget_args.chip_address->count) {
        if (0 ==
            sscanf(i2cget_args.chip_address->sval[0], "0x%hhx", &chip_addr)) {
            ESP_LOGE(TAG, "can not parsing chip_address");
            return ESP_ERR_INVALID_ARG;
        }
    } else {
        ESP_LOGE(TAG, "no input chip_address");
        return ESP_ERR_INVALID_ARG;
    }

    /* Check register address: "-r" option */
    uint8_t data_addr = 0;
    if (i2cget_args.register_address->count) {
        if (0 == sscanf(i2cget_args.register_address->sval[0], "0x%hhx",
                        &data_addr)) {
            ESP_LOGE(TAG, "can not parsing register_address");
            return ESP_ERR_INVALID_ARG;
        }
    } else {
        ESP_LOGE(TAG, "no input register_address");
        return ESP_ERR_INVALID_ARG;
    }

    /* Check data length: "-l" option */
    int len = 1;
    if (i2cget_args.data_length->count) {
        len = i2cget_args.data_length->ival[0];
    }

    uint8_t *buf = malloc(len);

    ERROR_CHECK(ims_i2c_write_read(devs, chip_addr, &data_addr, 1, buf, len,
                                  GET_REG_TIMEOUT_MS));

    printf("i2c read addr:0x%02x reg:0x%02x\n\r", (unsigned int)chip_addr, (unsigned int)data_addr);
    for (int i = 0; i < len; i++) {
        printf("0x%02x ", *(buf + i));
    }
    printf("\n\r");

    free(buf);
    return 0;
}

static void register_i2cget(void) {
    i2cget_args.name = arg_str0("b", "bus", "<i2c-name>", "i2c-0, i2c-1, ...");
    i2cget_args.chip_address =
        arg_str0("c", "chip", "<chip_addr>",
                 "Specify the address of the chip on that bus");
    i2cget_args.register_address =
        arg_str0("r", "register", "<register_addr>",
                 "Specify the address on that chip to read from");
    i2cget_args.data_length =
        arg_int0("l", "length", "<length>",
                 "Specify the length to read from that data address");
    i2cget_args.end = arg_end(2);
    const esp_console_cmd_t i2cget_cmd = {
        .command = "i2cget",
        .help = "Read registers visible through the I2C bus",
        .hint = NULL,
        .func = &do_i2cget_cmd,
        .argtable = &i2cget_args};
    ESP_ERROR_CHECK(esp_console_cmd_register(&i2cget_cmd));
}

static struct {
    struct arg_str *name;
    struct arg_str *chip_address;
    struct arg_str *register_address;
    struct arg_str *data;
    struct arg_end *end;
} i2cset_args;

static int do_i2cset_cmd(int argc, char **argv) {
    int nerrors = arg_parse(argc, argv, (void **)&i2cset_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, i2cset_args.end, argv[0]);
        return 0;
    }

    const struct ims_device *devs = {NULL};

    if (i2cset_args.name->count == 1) {
        devs = board_get_device(i2cset_args.name->sval[0]);
        if (devs == NULL) {
            return ESP_ERR_NOT_FOUND;
        }
    } else {
        return ESP_ERR_NOT_FOUND;
    }

    /* Check chip address: "-c" option */
    uint8_t chip_addr = 0;
    if (i2cset_args.chip_address->count) {
        if (0 ==
            sscanf(i2cset_args.chip_address->sval[0], "0x%hhx", &chip_addr)) {
            ESP_LOGE(TAG, "can not parsing chip_address");
            return ESP_ERR_INVALID_ARG;
        }
    } else {
        ESP_LOGE(TAG, "no input chip_address");
        return ESP_ERR_INVALID_ARG;
    }

    /* Check register address: "-r" option */
    uint8_t data_addr = 0;
    if (i2cset_args.register_address->count) {
        if (0 == sscanf(i2cset_args.register_address->sval[0], "0x%hhx",
                        &data_addr)) {
            ESP_LOGE(TAG, "can not parsing register_address");
            return ESP_ERR_INVALID_ARG;
        }
    } else {
        ESP_LOGE(TAG, "no input register_address");
        return ESP_ERR_INVALID_ARG;
    }
    /* Check data: "-d" option */
    int len = i2cset_args.data->count;

    uint8_t *buf = malloc(len + 1);
    *buf = data_addr;

    for (int i = 0; i < len; i++) {
        if (0 == sscanf(i2cset_args.data->sval[i], "0x%hhx", (buf + 1 + i))) {
            ESP_LOGE(TAG, "can not parsing data");
            return ESP_ERR_INVALID_ARG;
        }
    }

    ERROR_CHECK(
        ims_i2c_write(devs, chip_addr, buf, len + 1, SET_REG_TIMEOUT_MS));

    printf("i2c write addr:0x%02x reg:0x%02x\n\r", (unsigned int)chip_addr, (unsigned int)*buf);
    for (int i = 0; i < len; i++) {
        printf("0x%02x ", *(buf + 1 + i));
    }
    printf("\n\r");

    free(buf);

    return 0;
}

static void register_i2cset(void) {
    i2cset_args.name = arg_str0("b", "bus", "<i2c-name>", "i2c-0, i2c-1, ...");
    i2cset_args.chip_address =
        arg_str0("c", "chip", "<chip_addr>",
                 "Specify the address of the chip on that bus");
    i2cset_args.register_address =
        arg_str0("r", "register", "<register_addr>",
                 "Specify the address on that chip to read from");
    i2cset_args.data =
        arg_strn(NULL, NULL, "<data>", 0, 256,
                 "Specify the data to write to that data address");
    i2cset_args.end = arg_end(3);
    const esp_console_cmd_t i2cset_cmd = {
        .command = "i2cset",
        .help = "Set registers visible through the I2C bus",
        .hint = NULL,
        .func = &do_i2cset_cmd,
        .argtable = &i2cset_args};
    ESP_ERROR_CHECK(esp_console_cmd_register(&i2cset_cmd));
}

static struct {
    struct arg_str *name;
    struct arg_end *end;
} i2c_scan_args;

void i2c_scan(const struct ims_device *dev) {
    printf("%s\n", dev->name);
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f");
    // Scan the existing i2c slave devices except the preserved address. Please
    // refer to https://www.nxp.com/docs/en/user-guide/UM10204.pdf 3.2.9.
    for (int i = 0; i < 0x78; i++) {
        if (i % 16 == 0)
            printf("\n%.2x:", i);

        if (i < 8) {
            printf("   ");
        } else if (ims_i2c_write(dev, i, NULL, 0, 10) == 0) {
            printf(" %.2x", i);
        } else {
            printf(" --");
        }
    }
    printf("\n");
}

static esp_err_t cmd_i2c_scan(int argc, char **argv) {
    int err = arg_parse(argc, argv, (void **)&i2c_scan_args);
    if (err != 0) {
        arg_print_errors(stderr, i2c_scan_args.end, argv[0]);
        return ESP_FAIL;
    }

    const struct ims_device *devs[I2C_NUM_MAX] = {NULL};

    int count = 0;
    if (i2c_scan_args.name->count > 0) {
        devs[0] = board_get_device(i2c_scan_args.name->sval[0]);
        if (devs[0] == NULL) {
            return ESP_ERR_NOT_FOUND;
        }
        count = 1;
    } else {
        count =
            board_get_device_by_type(BOARD_DEVICE_TYPE_I2C, devs, sizeof(devs));
    }

    for (int i = 0; i < count; i++) {
        i2c_scan(devs[i]);
    }
    return ESP_OK;
}

static void register_i2scan(void) {
    i2c_scan_args.name =
        arg_str0("b", "bus", "<i2c-name>", "i2c-0, i2c-1, ...");
    i2c_scan_args.end = arg_end(1);

    const esp_console_cmd_t cmd = {
        .command = "i2c_scan",
        .help = "Scan available i2c slave id",
        .hint = NULL,
        .func = &cmd_i2c_scan,
        .argtable = &i2c_scan_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

esp_err_t register_i2c_tool_command(void) {
    register_i2cget();
    register_i2cset();
    register_i2scan();
    return 0;
}
