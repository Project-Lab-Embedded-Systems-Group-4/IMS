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
#include "nvs.h"

static struct {
    struct arg_str *subcommand;
    struct arg_int *start;
    struct arg_int *inc;
    struct arg_int *num;
    struct arg_int *pga;
    struct arg_int *range;
    struct arg_int *fb;
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

    return 0;
}

static int do_ad5933_dump(void) {
    struct ad5933_sample_data *samples;
    double *gain_factors;
    uint16_t count;
    esp_err_t err = ad5933_service_get_results(&samples, &gain_factors, &count);

    if (err == ESP_ERR_INVALID_STATE) {
        printf("Error: Sweep in progress or service not initialized.\n");
        return 1;
    }

    if (count == 0) {
        printf("No data available. Run 'ad5933 sweep' first.\n");
        return 0;
    }

    printf("%-6s | %-8s | %-8s | %-12s | %-15s | %-15s\n", "Index", "Real", "Imag",
           "Magnitude", "Gain Factor", "Impedance (Ohm)");
    printf("-------|----------|----------|--------------|-----------------|----------------\n");
    for (int i = 0; i < count; i++) {
        double magnitude =
            sqrt((double)samples[i].real * samples[i].real +
                 (double)samples[i].imag * samples[i].imag);
        double gf = gain_factors[i];
        double impedance = 0;
        if (gf != 0 && magnitude != 0) {
            impedance = 1.0 / (gf * magnitude);
        }
        printf("%-6d | %-8d | %-8d | %-12.2f | %-15.2e | %-15.2f\n", i, samples[i].real,
               samples[i].imag, magnitude, gf, impedance);
    }

    return 0;
}

static int do_ad5933_cal(int fb_index) {
    extern esp_event_loop_handle_t service_event_loop;
    
    if (fb_index < 0 || fb_index > 3) {
        printf("Error: FB index must be 0-3\n");
        return 1;
    }

    uint8_t fb = (uint8_t)fb_index;
    esp_err_t err = esp_event_post_to(
        service_event_loop, IMS_EVENT_BASE, IMS_EVENT_AD5933_START_CAL,
        &fb, sizeof(fb), portMAX_DELAY);
    
    if (err == ESP_OK) {
        printf("AD5933 Calibration requested for FB index %d...\n", fb_index);
    } else {
        printf("Failed to request calibration: %s\n", esp_err_to_name(err));
    }
    return 0;
}

static int do_ad5933_cmd(int argc, char **argv) {
    bool continuous = false;
    int interval_ms = 1000;
    int average_times = 1;

    if (argc >= 2 && strcmp(argv[1], "sweep") == 0) {
        for (int i = 2; i < argc; ) {
            if (strcmp(argv[i], "-c") == 0) {
                continuous = true;
                for (int j = i; j < argc - 1; j++) {
                    argv[j] = argv[j + 1];
                }
                argc--;
            } else if (strcmp(argv[i], "-i") == 0) {
                if (i + 1 < argc) {
                    interval_ms = atoi(argv[i+1]);
                    for (int j = i; j < argc - 2; j++) {
                        argv[j] = argv[j + 2];
                    }
                    argc -= 2;
                } else {
                    printf("Error: -i requires an integer value (interval in ms)\n");
                    return 1;
                }
            } else if (strcmp(argv[i], "-a") == 0) {
                if (i + 1 < argc) {
                    average_times = atoi(argv[i+1]);
                    for (int j = i; j < argc - 2; j++) {
                        argv[j] = argv[j + 2];
                    }
                    argc -= 2;
                } else {
                    printf("Error: -a requires an integer value\n");
                    return 1;
                }
            } else {
                i++;
            }
        }
    }

    PARSE_ARG(ad5933_args);

    const char *sub = ad5933_args.subcommand->sval[0];
    const struct ims_device *dev = board_get_device("ad5933");
    if (!dev) {
        printf("AD5933 device not found\n");
        return 1;
    }

    if (strcmp(sub, "help") == 0) {
        printf("Usage: ad5933 <subcommand> [options]\n\n");
        printf("Subcommands:\n");
        printf("  info                 Show current AD5933 status and configuration\n");
        printf("  sweep [options]      Perform a frequency sweep\n");
        printf("                       -c: continuously sweep in the background\n");
        printf("                       -i <ms>: interval between continuous sweeps (default: 1000)\n");
        printf("                       -a <count>: number of points to sweep and average (sets num increments)\n");
        printf("  dump                 Display results of the last sweep\n");
        printf("  cal [-f <0-3>]       Start calibration using feedback resistor index\n");
        printf("                       -f: feedback index (0:2k, 1:10k, 2:100k, 3:330k)\n");
        printf("  set [options]        Set configuration parameters\n");
        printf("                       -s, --start=<Hz>: start frequency\n");
        printf("                       -i, --inc=<Hz>: increment frequency\n");
        printf("                       -n, --num=<n>: number of increments (0-511)\n");
        printf("                       -p, --pga=<0|1>: PGA gain (0=x5, 1=x1)\n");
        printf("                       -r, --range=<0-3>: excitation voltage range\n");
        printf("  reset                Perform a hardware reset\n");
        printf("  stop                 Stop the active continuous sweep\n");
        return 0;
    } else if (strcmp(sub, "info") == 0) {
        return do_ad5933_info(dev);
    } else if (strcmp(sub, "reset") == 0) {
        ad5933_reset(dev);
        printf("AD5933 reset performed.\n");
        return 0;
    } else if (strcmp(sub, "stop") == 0) {
        extern esp_event_loop_handle_t service_event_loop;
        esp_err_t err = esp_event_post_to(
            service_event_loop, IMS_EVENT_BASE, IMS_EVENT_AD5933_STOP_SWEEP,
            NULL, 0, portMAX_DELAY);
        if (err == ESP_OK) {
            printf("Stop request sent to sweep service...\n");
        } else {
            printf("Failed to send stop request: %s\n", esp_err_to_name(err));
        }
        return 0;
    } else if (strcmp(sub, "sweep") == 0) {
        struct ad5933_sweep_params params = {
            .continuous = continuous,
            .interval_ms = interval_ms,
            .average_times = average_times,
        };

        extern esp_event_loop_handle_t service_event_loop;
        esp_err_t err = esp_event_post_to(
            service_event_loop, IMS_EVENT_BASE, IMS_EVENT_AD5933_START_SWEEP,
            &params, sizeof(params), portMAX_DELAY);
        if (err == ESP_OK) {
            if (continuous) {
                printf("Continuous sweep started (interval: %d ms, avg: %d). Press ENTER to stop...\n", interval_ms, average_times);
                while (1) {
                    int c = getchar();
                    if (c == '\n' || c == '\r') {
                        break;
                    }
                    if (c == EOF) {
                        vTaskDelay(pdMS_TO_TICKS(50));
                    }
                }
                // Send stop request
                esp_event_post_to(
                    service_event_loop, IMS_EVENT_BASE, IMS_EVENT_AD5933_STOP_SWEEP,
                    NULL, 0, portMAX_DELAY);
            } else {
                printf("Sweep started...\n");
            }
        } else {
            printf("Failed to start sweep: %s\n", esp_err_to_name(err));
        }
        return 0;
    } else if (strcmp(sub, "cal") == 0) {
        int fb = 1; // Default FB 1
        if (ad5933_args.fb->count > 0) {
            fb = ad5933_args.fb->ival[0];
        }
        return do_ad5933_cal(fb);
    } else if (strcmp(sub, "dump") == 0) {
        return do_ad5933_dump();
    } else if (strcmp(sub, "set") == 0) {
        nvs_handle_t my_handle;
        bool has_changes = false;
        esp_err_t nvs_err = nvs_open("ad5933", NVS_READWRITE, &my_handle);

        if (ad5933_args.start->count) {
            uint32_t val = ad5933_args.start->ival[0];
            ad5933_set_start_freq(dev, val);
            if (nvs_err == ESP_OK) {
                nvs_set_u32(my_handle, "start_freq", val);
                has_changes = true;
            }
        }
        if (ad5933_args.inc->count) {
            uint32_t val = ad5933_args.inc->ival[0];
            ad5933_set_inc_freq(dev, val);
            if (nvs_err == ESP_OK) {
                nvs_set_u32(my_handle, "inc_freq", val);
                has_changes = true;
            }
        }
        if (ad5933_args.num->count) {
            uint16_t val = (uint16_t)ad5933_args.num->ival[0];
            ad5933_set_num_inc(dev, val);
            if (nvs_err == ESP_OK) {
                nvs_set_u16(my_handle, "num_inc", val);
                has_changes = true;
            }
        }
        if (ad5933_args.pga->count) {
            int pga = ad5933_args.pga->ival[0];
            if (pga == 0 || pga == 1) {
                ad5933_set_pga_gain(dev, (enum ad5933_pga_gain)pga);
                if (nvs_err == ESP_OK) {
                    nvs_set_u8(my_handle, "pga_gain", (uint8_t)pga);
                    has_changes = true;
                }
            } else {
                printf("Error: PGA must be 0 (x5) or 1 (x1)\n");
            }
        }
        if (ad5933_args.range->count) {
            int r = ad5933_args.range->ival[0];
            if (r >= 0 && r <= 3) {
                ad5933_set_voltage_range(dev, (enum ad5933_voltage_range)r);
                if (nvs_err == ESP_OK) {
                    nvs_set_u8(my_handle, "voltage_range", (uint8_t)r);
                    has_changes = true;
                }
            } else {
                printf("Error: Range must be 0..3\n");
            }
        }
        if (nvs_err == ESP_OK) {
            if (has_changes) {
                nvs_commit(my_handle);
            }
            nvs_close(my_handle);
        }
        printf("Configuration updated.\n");
        return 0;
    } else {
        printf("Unknown subcommand: %s. Use info, sweep, dump, cal, set, reset, stop, or help.\n", sub);
        return 1;
    }
}

esp_err_t register_ad5933_command(void) {
    ad5933_args.subcommand =
        arg_str1(NULL, NULL, "<info|sweep|dump|cal|set|reset|stop|help>", "Sub-command to execute");
    ad5933_args.start = arg_int0("s", "start", "<Hz>", "Start frequency in Hz");
    ad5933_args.inc =
        arg_int0("i", "inc", "<Hz>", "Increment frequency in Hz");
    ad5933_args.num =
        arg_int0("n", "num", "<n>", "Number of increments (0-511)");
    ad5933_args.pga =
        arg_int0("p", "pga", "<0|1>", "PGA Gain: 0=x5, 1=x1 (for 'set')");
    ad5933_args.range = arg_int0("r", "range", "<0-3>",
                                 "Range: 0=2V, 1=200mV, 2=400mV, 3=1V");
    ad5933_args.fb = arg_int0("f", "fb", "<0-3>", "ZM Feedback index (for 'cal'): 0:2k, 1:10k, 2:100k, 3:330k");
    ad5933_args.end = arg_end(8);

    const esp_console_cmd_t cmd = {
        .command = "ad5933",
        .help = "AD5933 control commands. Use 'ad5933 help' for detailed subcommand/sweep options.",
        .hint = NULL,
        .func = &do_ad5933_cmd,
        .argtable = &ad5933_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));

    return ESP_OK;
}
