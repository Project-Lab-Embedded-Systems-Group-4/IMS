#include <argtable3/argtable3.h>
#include <esp_console.h>
#include <esp_event.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "board/board.h"
#include "cmd.h"
#include "event.h"
#include "board_utils.h"
#include "services/ad5933/ad5933_service.h"
#include "nvs.h"

static void trim_trailing(char *str) {
    int len = strlen(str);
    while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\t' || str[len - 1] == '\r' || str[len - 1] == '\n')) {
        str[len - 1] = '\0';
        len--;
    }
}

static void get_line_interactive(char *buf, size_t max_len) {
    size_t index = 0;
    while (index < max_len - 1) {
        int c = getchar();
        if (c == EOF) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        if (c == '\r' || c == '\n') {
            printf("\n");
            break;
        }
        if (c == '\b' || c == 127) {
            if (index > 0) {
                index--;
                printf("\b \b");
                fflush(stdout);
            }
            continue;
        }
        if (c >= 32 && c <= 126) {
            buf[index++] = c;
            putchar(c);
            fflush(stdout);
        }
    }
    buf[index] = '\0';
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static struct {
    struct arg_str *subcommand;
    struct arg_int *start;
    struct arg_int *inc;
    struct arg_int *num;
    struct arg_int *pga;
    struct arg_int *range;
    struct arg_int *fb;
    struct arg_int *offset_ch;
    struct arg_dbl *offset_val;
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

    const struct ims_device *dev = board_get_device("ad5933");
    if (!dev) {
        printf("AD5933 device not found\n");
        return 1;
    }
    uint32_t start_freq = 0;
    ad5933_get_start_freq(dev, &start_freq);

    struct board_utils_info board_info;
    board_utils_get_info(&board_info);
    double offsets[10] = {0};
    ad5933_service_get_offsets(offsets, 10);
    double offset = 0;
    if (board_info.subj_channel >= 0 && board_info.subj_channel <= 9) {
        offset = offsets[board_info.subj_channel];
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
            double raw_impedance = 1.0 / (gf * magnitude);
            if (offset > 0.0) {
                double f = (double)start_freq;
                if (f > 0.0) {
                    double C_pF = 1e12 / (2.0 * M_PI * f * raw_impedance);
                    double C_corrected_pf = C_pF - offset;
                    if (C_corrected_pf > 0.0) {
                        impedance = 1e12 / (2.0 * M_PI * f * C_corrected_pf);
                    } else {
                        impedance = INFINITY;
                    }
                } else {
                    impedance = raw_impedance;
                }
            } else {
                impedance = raw_impedance;
            }
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
        printf("  offset [options]     Set or get channel capacitance offsets\n");
        printf("                       -c, --offset-ch=<1-10>: active subject channel\n");
        printf("                       -o, --offset-val=<pF>: offset value in pF\n");
        printf("  prep                 Run interactive calibration and offset preparation wizard\n");
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
    } else if (strcmp(sub, "offset") == 0) {
        if (ad5933_args.offset_ch->count && ad5933_args.offset_val->count) {
            int ch = ad5933_args.offset_ch->ival[0];
            double val = ad5933_args.offset_val->dval[0];
            if (ch < 1 || ch > 10) {
                printf("Error: Channel must be 1-10\n");
                return 1;
            }
            if (val < 0) {
                printf("Error: Offset must be non-negative\n");
                return 1;
            }
            esp_err_t err = ad5933_service_set_offset((uint8_t)(ch - 1), val);
            if (err == ESP_OK) {
                printf("Offset for channel %d set to %.2f pF.\n", ch, val);
            } else {
                printf("Failed to set offset: %s\n", esp_err_to_name(err));
            }
        } else {
            // Print all offsets
            double offsets[10] = {0};
            esp_err_t err = ad5933_service_get_offsets(offsets, 10);
            if (err == ESP_OK) {
                printf("AD5933 Channel Capacitance Offsets:\n");
                for (int i = 0; i < 10; i++) {
                    printf("\tChannel %2d: %8.2f pF\n", i + 1, offsets[i]);
                }
            } else {
                printf("Failed to get offsets: %s\n", esp_err_to_name(err));
            }
        }
        return 0;
    } else if (strcmp(sub, "prep") == 0) {
        char input_buf[64];
        
        // 1. start_freq
        uint32_t start_freq = 10000;
        printf("Enter start frequency in Hz [default: 10000]: ");
        fflush(stdout);
        get_line_interactive(input_buf, sizeof(input_buf));
        trim_trailing(input_buf);
        if (strlen(input_buf) > 0) {
            uint32_t val = strtoul(input_buf, NULL, 10);
            if (val > 0) {
                start_freq = val;
            } else {
                printf("Invalid frequency, using default: %" PRIu32 " Hz\n", start_freq);
            }
        }
        
        // 2. Reference resistor index
        int ref_res_idx = 3;
        printf("Enter reference resistor index (0: 2k, 1: 10k, 2: 100k, 3: 330k) [default: 3]: ");
        fflush(stdout);
        get_line_interactive(input_buf, sizeof(input_buf));
        trim_trailing(input_buf);
        if (strlen(input_buf) > 0) {
            int val = atoi(input_buf);
            if (val >= 0 && val <= 3) {
                ref_res_idx = val;
            } else {
                printf("Invalid reference index, using default: %d\n", ref_res_idx);
            }
        }
        
        // 3. Subject channel
        int subject_channel = 1;
        bool all_channels = false;
        printf("Enter subject channel (1-10 or 'all') [default: 1]: ");
        fflush(stdout);
        get_line_interactive(input_buf, sizeof(input_buf));
        trim_trailing(input_buf);
        if (strlen(input_buf) > 0) {
            if (strcmp(input_buf, "all") == 0) {
                all_channels = true;
            } else {
                int val = atoi(input_buf);
                if (val >= 1 && val <= 10) {
                    subject_channel = val;
                } else {
                    printf("Invalid channel, using default: %d\n", subject_channel);
                }
            }
        }
        
        // 4. Averaging iteration count
        int avg_times = 10;
        printf("Enter averaging iteration count [default: 10]: ");
        fflush(stdout);
        get_line_interactive(input_buf, sizeof(input_buf));
        trim_trailing(input_buf);
        if (strlen(input_buf) > 0) {
            int val = atoi(input_buf);
            if (val > 0) {
                avg_times = val;
            } else {
                printf("Invalid iteration count, using default: %d\n", avg_times);
            }
        }
        
        // 5. Warning and confirmation
        printf("\nWARNING: Please disconnect all DUTs from the board before proceeding.\n");
        printf("Press ENTER to start preparation wizard...");
        fflush(stdout);
        get_line_interactive(input_buf, sizeof(input_buf));

        // Configure parameters on device
        ad5933_set_start_freq(dev, start_freq);
        ad5933_set_inc_freq(dev, 0);
        ad5933_set_num_inc(dev, avg_times > 0 ? avg_times - 1 : 0);

        // Post calibration request
        extern esp_event_loop_handle_t service_event_loop;
        uint8_t fb = (uint8_t)ref_res_idx;
        esp_err_t err = esp_event_post_to(
            service_event_loop, IMS_EVENT_BASE, IMS_EVENT_AD5933_START_CAL,
            &fb, sizeof(fb), portMAX_DELAY);
        if (err != ESP_OK) {
            printf("ERROR: Failed to post calibration event: %s\n", esp_err_to_name(err));
            return 1;
        }

        printf("Calibrating on reference resistor %d...\n", ref_res_idx);
        vTaskDelay(pdMS_TO_TICKS(200));
        struct ad5933_sample_data *dummy_samples;
        double *dummy_gf;
        uint16_t dummy_count;
        while (ad5933_service_get_results(&dummy_samples, &dummy_gf, &dummy_count) != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        printf("Calibration completed.\n");

        int start_ch = all_channels ? 0 : (subject_channel - 1);
        int end_ch = all_channels ? 9 : (subject_channel - 1);

        for (int ch = start_ch; ch <= end_ch; ch++) {
            printf("Measuring open-circuit stray capacitance on Channel %d...\n", ch + 1);
            board_set_subj_channel((enum board_subj_channel)ch);

            struct ad5933_sweep_params params = {
                .continuous = false,
                .interval_ms = 50,
                .average_times = avg_times,
            };

            err = esp_event_post_to(
                service_event_loop, IMS_EVENT_BASE, IMS_EVENT_AD5933_START_SWEEP,
                &params, sizeof(params), portMAX_DELAY);
            if (err != ESP_OK) {
                printf("ERROR: Failed to post sweep event for channel %d: %s\n", ch + 1, esp_err_to_name(err));
                continue;
            }

            vTaskDelay(pdMS_TO_TICKS(1000));
            struct ad5933_sample_data *samples;
            double *gain_factors;
            uint16_t count;
            while (ad5933_service_get_results(&samples, &gain_factors, &count) != ESP_OK) {
                vTaskDelay(pdMS_TO_TICKS(100));
            }

            double sum_raw_impedance = 0;
            int valid_count = 0;
            for (int i = 0; i < count; i++) {
                double magnitude = sqrt((double)samples[i].real * samples[i].real +
                                        (double)samples[i].imag * samples[i].imag);
                double gf = gain_factors[i];
                if (gf != 0 && magnitude != 0) {
                    sum_raw_impedance += 1.0 / (gf * magnitude);
                    valid_count++;
                }
            }

            if (valid_count > 0) {
                double avg_raw_impedance = sum_raw_impedance / valid_count;
                double f = (double)start_freq;
                double C_pF = 0;
                if (f > 0.0 && avg_raw_impedance > 0.0) {
                    C_pF = 1e12 / (2.0 * M_PI * f * avg_raw_impedance);
                }

                err = ad5933_service_set_offset((uint8_t)ch, C_pF);
                if (err == ESP_OK) {
                    printf("SUCCESS: Offset for channel %d successfully set to %.2f pF and saved to flash.\n", ch + 1, C_pF);
                } else {
                    printf("ERROR: Failed to save offset for channel %d: %s\n", ch + 1, esp_err_to_name(err));
                }
            } else {
                printf("ERROR: No valid sweep data received for channel %d.\n", ch + 1);
            }
        }
        return 0;
    } else {
        printf("Unknown subcommand: %s. Use info, sweep, dump, cal, set, reset, stop, offset, prep, or help.\n", sub);
        return 1;
    }
}

esp_err_t register_ad5933_command(void) {
    ad5933_args.subcommand =
        arg_str1(NULL, NULL, "<info|sweep|dump|cal|set|reset|stop|offset|prep|help>", "Sub-command to execute");
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
    ad5933_args.offset_ch = arg_int0("c", "offset-ch", "<1-10>", "Subject channel (1-10) for offset");
    ad5933_args.offset_val = arg_dbl0("o", "offset-val", "<pF>", "Offset capacitance in pF");
    ad5933_args.end = arg_end(10);

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
