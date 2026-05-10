#ifndef IMS_CMD_CMD_H_
#define IMS_CMD_CMD_H_

#include <esp_err.h>
#include <stdio.h>
#include <argtable3/argtable3.h>
#include "board/board.h"

// NOTE: must have name argument in args
#define PREPARE_DEV(args, dev, default_dev_name)                               \
    do {                                                                       \
        int err = arg_parse(argc, argv, (void **)&(args));                     \
        if (err != 0) {                                                        \
            arg_print_errors(stderr, args.end, argv[0]);                       \
            return ESP_ERR_INVALID_ARG;                                        \
        }                                                                      \
                                                                               \
        const char *dev_name = default_dev_name;                               \
        if (args.name->count > 0) {                                            \
            dev_name = args.name->sval[0];                                     \
        }                                                                      \
        if (dev_name == NULL) {                                                \
            printf("%s: device not found: name=%s\n", __FUNCTION__, dev_name); \
            return ESP_ERR_NOT_FOUND;                                          \
        }                                                                      \
                                                                               \
        dev = board_get_device(dev_name);                                      \
        if (dev == NULL) {                                                     \
            printf("%s: device not found\n", __FUNCTION__);                    \
            return ESP_ERR_NOT_FOUND;                                          \
        }                                                                      \
    } while (0)

#define PARSE_ARG(args)                                                        \
    do {                                                                       \
        int err = arg_parse(argc, argv, (void **)&(args));                     \
        if (err != 0) {                                                        \
            arg_print_errors(stderr, args.end, argv[0]);                       \
            return ESP_ERR_INVALID_ARG;                                        \
        }                                                                      \
    } while (0)

esp_err_t register_system(void);
esp_err_t register_i2c_tool_command(void);
esp_err_t register_gpio_pin_command(size_t max_num);
esp_err_t register_ad5933_command(void);
esp_err_t register_board_utils_command(void);

#endif // IMS_CMD_CMD_H_
