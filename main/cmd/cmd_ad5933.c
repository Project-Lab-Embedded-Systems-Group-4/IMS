#include <argtable3/argtable3.h>
#include <esp_console.h>
#include <esp_event.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "board/board.h"
#include "cmd.h"
#include "event.h"
#include "services/ad5933/ad5933_service.h"

static struct {
    struct arg_end *end;
} sweep_args;

static int ad5933_sweep(int argc, char **argv) {
    PARSE_ARG(sweep_args);
    extern esp_event_loop_handle_t service_event_loop;

    esp_err_t err =
        esp_event_post_to(service_event_loop, IMS_EVENT_BASE,
                          IMS_EVENT_AD5933_START_SWEEP, NULL, 0, portMAX_DELAY);
    if (err == ESP_OK) {
        printf("Sweep started...\n");
    } else {
        printf("Failed to start sweep: %s\n", esp_err_to_name(err));
    }
    return 0;
}

static struct {
    struct arg_end *end;
} dump_args;

static int ad5933_dump(int argc, char **argv) {
    PARSE_ARG(dump_args);

    struct ad5933_sample_data *samples;
    uint16_t count;
    esp_err_t err = ad5933_service_get_results(&samples, &count);

    if (err == ESP_ERR_INVALID_STATE) {
        printf("Error: Sweep in progress or service not initialized.\n");
        return 1;
    }

    if (count == 0) {
        printf("No data available. Run 'ad5933_sweep' first.\n");
        return 0;
    }

    const struct ims_device *dev = board_get_device("ad5933");
    struct ad5933_data *drv_data = (struct ad5933_data *)dev->data;
    double gf = drv_data->gain_factor;

    printf("Gain Factor: %e\n", gf);
    printf("Index, Real, Imag, Magnitude, Impedance (Ohm)\n");
    for (int i = 0; i < count; i++) {
        double magnitude = sqrt((double)samples[i].real * samples[i].real +
                                (double)samples[i].imag * samples[i].imag);
        double impedance = 0;
        if (gf != 0 && magnitude != 0) {
            impedance = 1.0 / (gf * magnitude);
        }
        printf("%d, %d, %d, %.2f, %.2f\n", i, samples[i].real, samples[i].imag,
               magnitude, impedance);
    }

    return 0;
}

static struct {
    struct arg_end *end;
} info_args;

static int ad5933_info(int argc, char **argv) {
    PARSE_ARG(info_args);
    const struct ims_device *dev = board_get_device("ad5933");
    if (!dev) {
        printf("AD5933 device not found\n");
        return 1;
    }
    struct ad5933_data *d = (struct ad5933_data *)dev->data;
    printf("AD5933 Status:\n");
    printf("\tClock Freq: %" PRIu32 " Hz\n", d->clock_freq);
    printf("\tStart Freq: %" PRIu32 " Hz\n", d->start_freq);
    printf("\tInc Freq:   %" PRIu32 " Hz\n", d->inc_freq);
    printf("\tNum Inc:    %u\n", d->num_inc);
    printf("\tSettling:   %u cycles (multiplier x%d)\n", d->settling_cycles,
           (d->settle_mul == AD5933_SETTLE_X1)
               ? 1
               : (d->settle_mul == AD5933_SETTLE_X2 ? 2 : 4));
    printf("\tGain Factor: %e\n", d->gain_factor);
    printf("\tPGA Gain:    %s\n",
           (d->ctrl1.pga_gain == AD5933_PGA_GAIN_X1) ? "x1" : "x5");

    const char *range_str = "Unknown";
    switch (d->ctrl1.output_voltage_range) {
    case AD5933_RANGE_2V_PP:
        range_str = "2.0V p-p";
        break;
    case AD5933_RANGE_200MV_PP:
        range_str = "200mV p-p";
        break;
    case AD5933_RANGE_400MV_PP:
        range_str = "400mV p-p";
        break;
    case AD5933_RANGE_1V_PP:
        range_str = "1.0V p-p";
        break;
    }
    printf("\tVoltage:     %s\n", range_str);
    return 0;
}

static struct {
    struct arg_dbl *gain;
    struct arg_end *end;
} set_gain_args;

static int ad5933_set_gain_val(int argc, char **argv) {
    PARSE_ARG(set_gain_args);
    const struct ims_device *dev = board_get_device("ad5933");
    ad5933_set_gain_factor(dev, set_gain_args.gain->dval[0]);
    printf("Gain factor set to %e\n", set_gain_args.gain->dval[0]);
    return 0;
}

static struct {
    struct arg_int *start;
    struct arg_int *inc;
    struct arg_int *num;
    struct arg_end *end;
} set_sweep_args;

static int ad5933_set_sweep(int argc, char **argv) {
    PARSE_ARG(set_sweep_args);
    const struct ims_device *dev = board_get_device("ad5933");
    if (set_sweep_args.start->count)
        ad5933_set_start_freq(dev, set_sweep_args.start->ival[0]);
    if (set_sweep_args.inc->count)
        ad5933_set_inc_freq(dev, set_sweep_args.inc->ival[0]);
    if (set_sweep_args.num->count)
        ad5933_set_num_inc(dev, (uint16_t)set_sweep_args.num->ival[0]);
    printf("Sweep parameters updated.\n");
    return 0;
}

static struct {
    struct arg_int *pga;
    struct arg_end *end;
} set_pga_args;

static int ad5933_set_pga_val(int argc, char **argv) {
    PARSE_ARG(set_pga_args);
    const struct ims_device *dev = board_get_device("ad5933");
    int pga = set_pga_args.pga->ival[0];
    if (pga != 0 && pga != 1) {
        printf("Error: PGA must be 0 (x5) or 1 (x1)\n");
        return 1;
    }
    ad5933_set_pga_gain(dev, (enum ad5933_pga_gain)pga);
    printf("PGA Gain set to %s\n", pga ? "x1" : "x5");
    return 0;
}

static struct {
    struct arg_int *range;
    struct arg_end *end;
} set_range_args;

static int ad5933_set_range_val(int argc, char **argv) {
    PARSE_ARG(set_range_args);
    const struct ims_device *dev = board_get_device("ad5933");
    int r = set_range_args.range->ival[0];
    if (r < 0 || r > 3) {
        printf("Error: Range must be 0..3\n");
        return 1;
    }
    ad5933_set_voltage_range(dev, (enum ad5933_voltage_range)r);
    printf("Voltage range updated.\n");
    return 0;
}

esp_err_t register_ad5933_command(void) {
    sweep_args.end = arg_end(0);
    const esp_console_cmd_t sweep_cmd = {
        .command = "ad5933_sweep",
        .help = "Start AD5933 frequency sweep",
        .hint = NULL,
        .func = &ad5933_sweep,
        .argtable = &sweep_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&sweep_cmd));

    dump_args.end = arg_end(0);
    const esp_console_cmd_t dump_cmd = {
        .command = "ad5933_dump",
        .help = "Dump last sweep results with impedance",
        .hint = NULL,
        .func = &ad5933_dump,
        .argtable = &dump_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&dump_cmd));

    info_args.end = arg_end(0);
    const esp_console_cmd_t info_cmd = {
        .command = "ad5933_info",
        .help = "Get AD5933 configuration info",
        .hint = NULL,
        .func = &ad5933_info,
        .argtable = &info_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&info_cmd));

    set_gain_args.gain =
        arg_dbl1(NULL, NULL, "<g>", "Gain factor (e.g. 5.15e-10)");
    set_gain_args.end = arg_end(1);
    const esp_console_cmd_t set_gain_cmd = {
        .command = "ad5933_set_gain",
        .help = "Set AD5933 gain factor",
        .hint = NULL,
        .func = &ad5933_set_gain_val,
        .argtable = &set_gain_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_gain_cmd));

    set_sweep_args.start =
        arg_int0("s", "start", "<Hz>", "Start frequency in Hz");
    set_sweep_args.inc =
        arg_int0("i", "inc", "<Hz>", "Increment frequency in Hz");
    set_sweep_args.num =
        arg_int0("n", "num", "<n>", "Number of increments (0-511)");
    set_sweep_args.end = arg_end(3);
    const esp_console_cmd_t set_sweep_cmd = {
        .command = "ad5933_set_sweep",
        .help = "Set sweep parameters",
        .hint = NULL,
        .func = &ad5933_set_sweep,
        .argtable = &set_sweep_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_sweep_cmd));

    set_pga_args.pga =
        arg_int1(NULL, NULL, "<0|1>", "PGA Gain: 0 = x5, 1 = x1");
    set_pga_args.end = arg_end(1);
    const esp_console_cmd_t set_pga_cmd = {
        .command = "ad5933_set_pga",
        .help = "Set PGA gain",
        .hint = NULL,
        .func = &ad5933_set_pga_val,
        .argtable = &set_pga_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_pga_cmd));

    set_range_args.range = arg_int1(NULL, NULL, "<0-3>",
                                    "Range: 0=2.0V, 1=200mV, 2=400mV, 3=1.0V");
    set_range_args.end = arg_end(1);
    const esp_console_cmd_t set_range_cmd = {
        .command = "ad5933_set_range",
        .help = "Set output voltage range",
        .hint = NULL,
        .func = &ad5933_set_range_val,
        .argtable = &set_range_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_range_cmd));

    return ESP_OK;
}
