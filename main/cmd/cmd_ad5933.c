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
    struct arg_str *subcommand;
    struct arg_int *start;
    struct arg_int *inc;
    struct arg_int *num;
    struct arg_dbl *gain;
    struct arg_int *pga;
    struct arg_int *range;
    struct arg_end *end;
} ad5933_args;

static int do_ad5933_info(const struct ims_device *dev) {
    struct ad5933_data *d = (struct ad5933_data *)dev->data;

    /* Read Latest Data from Hardware */
    uint32_t start_freq, inc_freq;
    uint16_t num_inc, settle_cycles;
    enum ad5933_settle_mul settle_mul;
    ad5933_ctrl_reg1_t ctrl1;
    ad5933_ctrl_reg2_t ctrl2;
    ad5933_status_reg_t status;
    float temp = ad5933_get_temperature(dev);

    ad5933_get_start_freq(dev, &start_freq);
    ad5933_get_inc_freq(dev, &inc_freq);
    ad5933_get_num_inc(dev, &num_inc);
    ad5933_get_settling_cycles(dev, &settle_cycles, &settle_mul);
    ad5933_get_ctrl_reg1(dev, &ctrl1);
    ad5933_get_ctrl_reg2(dev, &ctrl2);
    ad5933_get_status_reg(dev, &status);

    printf("AD5933 Latest Hardware Info:\n");
    printf("\tTemperature: %.2f C\n", temp);
    printf("\tStatus:      TempValid=%d, DataValid=%d, SweepDone=%d\n",
           status.temp_valid, status.data_valid, status.sweep_complete);
    printf("\tClock Freq:  %" PRIu32 " Hz (%s)\n", d->clock_freq,
           ctrl2.clock_source == AD5933_CLK_EXT ? "External" : "Internal");
    printf("\tStart Freq:  %" PRIu32 " Hz\n", start_freq);
    printf("\tInc Freq:    %" PRIu32 " Hz\n", inc_freq);
    printf("\tNum Inc:     %u\n", num_inc);
    printf("\tSettling:    %u cycles (multiplier x%d)\n", settle_cycles,
           (settle_mul == AD5933_SETTLE_X1)
               ? 1
               : (settle_mul == AD5933_SETTLE_X2 ? 2 : 4));
    printf("\tPGA Gain:    %s\n",
           (ctrl1.pga_gain == AD5933_PGA_GAIN_X1) ? "x1" : "x5");

    const char *range_str = "Unknown";
    switch (ctrl1.output_voltage_range) {
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
    printf("\tGain Factor: %e (Stored)\n", d->gain_factor);

    return 0;
}

static int do_ad5933_dump(void) {
    struct ad5933_sample_data *samples;
    uint16_t count;
    esp_err_t err = ad5933_service_get_results(&samples, &count);

    if (err == ESP_ERR_INVALID_STATE) {
        printf("Error: Sweep in progress or service not initialized.\n");
        return 1;
    }

    if (count == 0) {
        printf("No data available. Run 'ad5933 sweep' first.\n");
        return 0;
    }

    const struct ims_device *dev = board_get_device("ad5933");
    struct ad5933_data *drv_data = (struct ad5933_data *)dev->data;
    double gf = drv_data->gain_factor;

    printf("Gain Factor: %e\n", gf);
    printf("%-6s | %-8s | %-8s | %-12s | %-15s\n", "Index", "Real", "Imag",
           "Magnitude", "Impedance (Ohm)");
    printf("-------|----------|----------|--------------|----------------\n");
    for (int i = 0; i < count; i++) {
        double magnitude =
            sqrt((double)samples[i].real * samples[i].real +
                 (double)samples[i].imag * samples[i].imag);
        double impedance = 0;
        if (gf != 0 && magnitude != 0) {
            impedance = 1.0 / (gf * magnitude);
        }
        printf("%-6d | %-8d | %-8d | %-12.2f | %-15.2f\n", i, samples[i].real,
               samples[i].imag, magnitude, impedance);
    }

    return 0;
}

static int do_ad5933_cmd(int argc, char **argv) {
    PARSE_ARG(ad5933_args);

    const char *sub = ad5933_args.subcommand->sval[0];
    const struct ims_device *dev = board_get_device("ad5933");
    if (!dev) {
        printf("AD5933 device not found\n");
        return 1;
    }

    if (strcmp(sub, "info") == 0) {
        return do_ad5933_info(dev);
    } else if (strcmp(sub, "reset") == 0) {
        ad5933_reset(dev);
        printf("AD5933 reset performed.\n");
        return 0;
    } else if (strcmp(sub, "sweep") == 0) {
        extern esp_event_loop_handle_t service_event_loop;
        esp_err_t err = esp_event_post_to(
            service_event_loop, IMS_EVENT_BASE, IMS_EVENT_AD5933_START_SWEEP,
            NULL, 0, portMAX_DELAY);
        if (err == ESP_OK) {
            printf("Sweep started...\n");
        } else {
            printf("Failed to start sweep: %s\n", esp_err_to_name(err));
        }
        return 0;
    } else if (strcmp(sub, "dump") == 0) {
        return do_ad5933_dump();
    } else if (strcmp(sub, "set") == 0) {
        if (ad5933_args.start->count)
            ad5933_set_start_freq(dev, ad5933_args.start->ival[0]);
        if (ad5933_args.inc->count)
            ad5933_set_inc_freq(dev, ad5933_args.inc->ival[0]);
        if (ad5933_args.num->count)
            ad5933_set_num_inc(dev, (uint16_t)ad5933_args.num->ival[0]);
        if (ad5933_args.gain->count)
            ad5933_set_gain_factor(dev, ad5933_args.gain->dval[0]);
        if (ad5933_args.pga->count) {
            int pga = ad5933_args.pga->ival[0];
            if (pga == 0 || pga == 1) {
                ad5933_set_pga_gain(dev, (enum ad5933_pga_gain)pga);
            } else {
                printf("Error: PGA must be 0 (x5) or 1 (x1)\n");
            }
        }
        if (ad5933_args.range->count) {
            int r = ad5933_args.range->ival[0];
            if (r >= 0 && r <= 3) {
                ad5933_set_voltage_range(dev, (enum ad5933_voltage_range)r);
            } else {
                printf("Error: Range must be 0..3\n");
            }
        }
        printf("Configuration updated.\n");
        return 0;
    } else {
        printf("Unknown subcommand: %s. Use info, sweep, dump, or set.\n", sub);
        return 1;
    }
}

esp_err_t register_ad5933_command(void) {
    ad5933_args.subcommand =
        arg_str1(NULL, NULL, "<info|sweep|dump|set|reset>", "Sub-command to execute");
    ad5933_args.start = arg_int0("s", "start", "<Hz>", "Start frequency in Hz");
    ad5933_args.inc =
        arg_int0("i", "inc", "<Hz>", "Increment frequency in Hz");
    ad5933_args.num =
        arg_int0("n", "num", "<n>", "Number of increments (0-511)");
    ad5933_args.gain = arg_dbl0("g", "gain", "<g>", "Gain factor (for 'set')");
    ad5933_args.pga =
        arg_int0("p", "pga", "<0|1>", "PGA Gain: 0=x5, 1=x1 (for 'set')");
    ad5933_args.range = arg_int0("r", "range", "<0-3>",
                                 "Range: 0=2V, 1=200mV, 2=400mV, 3=1V");
    ad5933_args.end = arg_end(7);

    const esp_console_cmd_t cmd = {
        .command = "ad5933",
        .help = "AD5933 control commands",
        .hint = NULL,
        .func = &do_ad5933_cmd,
        .argtable = &ad5933_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));

    return ESP_OK;
}
