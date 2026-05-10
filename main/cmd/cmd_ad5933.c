#include <argtable3/argtable3.h>
#include <esp_console.h>
#include <esp_event.h>
#include <stdio.h>
#include <string.h>

#include "cmd.h"
#include "event.h"
#include "services/ad5933/ad5933_service.h"

static struct {
    struct arg_end *end;
} sweep_args;

static int ad5933_sweep(int argc, char **argv) {
    PARSE_ARG(sweep_args);

    /* Post start sweep event to default loop if we didn't store the service loop, 
       but hello_world_main uses a custom loop. I'll need to fetch the loop handle 
       or use the system loop if possible. 
       Actually, ad5933_service was initialized with service_event_loop.
       I'll update hello_world_main to expose it or store it here.
    */
    extern esp_event_loop_handle_t service_event_loop;
    
    esp_err_t err = esp_event_post_to(service_event_loop, IMS_EVENT_BASE, 
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

    printf("Index, Real, Imag\n");
    for (int i = 0; i < count; i++) {
        printf("%d, %d, %d\n", i, samples[i].real, samples[i].imag);
    }

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
        .help = "Dump last sweep results",
        .hint = NULL,
        .func = &ad5933_dump,
        .argtable = &dump_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&dump_cmd));

    return ESP_OK;
}
