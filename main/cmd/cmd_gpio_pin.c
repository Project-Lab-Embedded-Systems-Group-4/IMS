#include <argtable3/argtable3.h>
#include <assert.h>
#include <esp_console.h>
#include <esp_err.h>
#include <stdlib.h>
#include <string.h>

#include "board/board.h"
#include "cmd.h"
#include "ims-mcu-driver/device.h"
#include "ims-mcu-driver/gpio.h"

static const char *TAG = "cmd_gpio";
static size_t max_number_of_gpio = 1;

struct {
    struct arg_str *name;
    struct arg_int *level;
    struct arg_end *end;
} gpio_args;

static int verify_args(void);
static int get_devices(const struct ims_device *devs[], size_t size);
static int get_gpio_pins(const struct ims_device *devs[], size_t size);
static int set_gpio_pins(const struct ims_device *devs[], size_t size);

static int operate_gpio_pin(int argc, char **argv) {
    int err = arg_parse(argc, argv, (void **)&gpio_args);
    if (err != 0) {
        arg_print_errors(stderr, gpio_args.end, argv[0]);
        return -1;
    }

    err = verify_args();
    if (err != 0) {
        arg_print_errors(stderr, gpio_args.end, argv[0]);
        return -1;
    }

    const struct ims_device **devs =
        calloc(max_number_of_gpio, sizeof(const struct ims_device *));
    if (devs == NULL) {
        ESP_LOGE(TAG, "memory not enough");
        return -1;
    }
    int n = get_devices(devs, max_number_of_gpio);
    if (n < 0) {
        goto FINAL;
    }
    if (n == 0) {
        ESP_LOGE(TAG, "no gpio available");
        goto FINAL;
    }

    if (gpio_args.level->count > 0) {
        err = set_gpio_pins(devs, n);
        if (err != 0) {
            goto FINAL;
        }
    }
    err = get_gpio_pins(devs, n);
    if (err != 0) {
        goto FINAL;
    }

FINAL:
    free(devs);
    return err;
}

static int verify_args(void) {
    int err = 0;
    for (int i = 0; i < gpio_args.level->count; i++) {
        int level = gpio_args.level->ival[0];
        if (level != 0 && level != 1) {
            ESP_LOGE(TAG, "-s: n=%d: level should be 0 or 1", i);
            err = -1;
        }
    }
    if (gpio_args.level->count > 0 &&
        gpio_args.name->count != gpio_args.level->count) {
        ESP_LOGE(TAG, "count of -n != count of -s");
        err = -1;
    }
    return err;
}

static int get_devices(const struct ims_device *devs[], size_t size) {
    if (gpio_args.name->count == 0) {
        int n =
            board_get_device_by_type(BOARD_DEVICE_TYPE_GPIO_PIN, devs, size);
        return n;
    }

    for (int i = 0; i < gpio_args.name->count; i++) {
        const char *dev_name = gpio_args.name->sval[i];
        devs[i] = board_get_device(dev_name);
        if (devs[i] == NULL) {
            ESP_LOGE(TAG, "device '%s' not exist", dev_name);
            return -1;
        }
    }
    return gpio_args.name->count;
}

static int get_gpio_pins(const struct ims_device *devs[], size_t size) {
    for (size_t i = 0; i < size; i++) {
        const struct ims_device *dev = devs[i];

        ims_gpio_flags_t flags;
        int err = ims_gpio_pin_get_config(dev, &flags);
        if (err != 0) {
            ESP_LOGW(TAG, "get: get pin configure: name=%s err=%d", dev->name,
                     err);
            continue;
        }
        int level = ims_gpio_pin_get(dev);
        if (level < 0) {
            ESP_LOGW(TAG, "get: value: name=%s err=%d", dev->name, level);
            continue;
        }
        ESP_LOGI(TAG, "%s: dir=%s level=%d", dev->name,
                 (flags & IMS_GPIO_FLAGS_INPUT) ? "IN" : "OUT", level);
    }
    return 0;
}

static int set_gpio_pins(const struct ims_device *devs[], size_t size) {
    for (size_t i = 0; i < size; i++) {
        const struct ims_device *dev = devs[i];
        const int level = gpio_args.level->ival[i];

        ims_gpio_flags_t flags;
        int err = ims_gpio_pin_get_config(dev, &flags);
        if (err != 0) {
            ESP_LOGE(TAG, "level: get pin configure: err=%d", err);
            return -1;
        }
        if ((flags & IMS_GPIO_FLAGS_OUTPUT) == 0) {
            ESP_LOGE(TAG, "level: pin '%s' is not output", dev->name);
            return -1;
        }
        err = ims_gpio_pin_set(dev, level);
        if (err != 0) {
            ESP_LOGE(TAG, "level: output: err=%d", err);
            return -1;
        }
        ESP_LOGI(TAG, "output %s level level: %d", dev->name, level);
    }
    return 0;
}

/********************** Global function definitions ************************/
esp_err_t register_gpio_pin_command(size_t max_num) {
    ESP_LOGD(TAG, "%s", __func__);
    assert(max_num >= 1);
    max_number_of_gpio = max_num;

    struct arg_str *name =
        arg_strn("n", "name", "<name>", 0, max_num, "device name");

    // gpio -g #  get all GPIO direction and value
    // gpio -n <pin_name> -l <0 or 1> # level one GPIO pin to 0 or 1
    // gpio -n <pin1_name> -l <0 or 1> -n <pin2_name> -l <0 or 1>
    gpio_args.name = name;
    gpio_args.level =
        arg_intn("l", "level", "<0|1>", 0, max_num, "Set output pin's level");
    gpio_args.end = arg_end(3);

    const esp_console_cmd_t cmds[] = {
        {
            .command = "gpio",
            // clang-format off
            .help = "Get/Set GPIO value\n"
                "  example:\n"
                "    gpio # get all GPIO direction and value\n"
                "    gpio -n <pin> ... # get specific GPIOs direction and value\n"
                "    gpio -n <pin> -l <0 or 1> -n <pin> -l <0 or 1> # level sepcific GPIOs value\n",
            // clang-format on
            .hint = NULL,
            .func = &operate_gpio_pin,
            .argtable = &gpio_args,
        },
    };

    for (int i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        esp_console_cmd_register(&cmds[i]);
    }

    return ESP_OK;
}
