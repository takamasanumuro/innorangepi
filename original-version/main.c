#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <limits.h>
#include <stdbool.h> // For bool type

#include "LineProtocol.h"
#include "CalibrationHelper.h"
#include "Measurement.h"
#include "util.h"
#include "ConfigurationLoader.h"
#include "ADS1115.h"
#include "ansi_colors.h"

// --- Constants ---
#define MEASUREMENT_DELAY_US 100000
#define MAIN_LOOP_DELAY_S 1

// --- Configuration Structs ---
// A dedicated struct to hold the filter configuration.
// This is populated once at startup to avoid repeated getenv() calls.
typedef struct {
    bool enabled;
    double alpha; // The filter coefficient (0.0 to 1.0)
} FilterConfig;

// --- Global variables for concurrency ---
pthread_mutex_t calibration_mutex = PTHREAD_MUTEX_INITIALIZER;
int g_calibration_sensor_index = -1;
volatile sig_atomic_t g_keep_running = 1;

// --- Function Prototypes ---
void signal_handler(int signum);
void loadFilterConfiguration(FilterConfig* config);
void run_measurement_loop(int i2c_handle, const InfluxDBContext* dbContext, const FilterConfig* filterConfig, Measurement* measurements, MeasurementSetting* settings);
void getMeasurements(int i2c_handle, const FilterConfig* filterConfig, const MeasurementSetting* settings, Measurement* measurements);
void applyConfigurations(const MeasurementSetting* settings, Measurement* measurements);
void printConfigurations(const char* config_file_str, const MeasurementSetting* settings);
void printMeasurements(const Measurement* measurements, const MeasurementSetting* settings);

// --- Implementations ---

void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        printf("\nTermination signal received. Shutting down...\n");
        g_keep_running = 0;
    }
}

// This function centralizes all logic for reading and validating filter settings from env vars.
// It is called only ONCE at startup.
void loadFilterConfiguration(FilterConfig* config) {
    // Set default values
    config->enabled = false;
    config->alpha = 0.2; // Default smoothing factor of 20%

    const char* should_filter_env = getenv("INSTRUMENTATION_FILTER_ADC_ENABLE");
    if (should_filter_env && strcmp(should_filter_env, "1") == 0) {
        config->enabled = true;
        printf("ADC filtering is " ANSI_COLOR_GREEN "ENABLED" ANSI_COLOR_RESET ". Readings will be smoothed.\n");

        const char* intensity_env = getenv("INSTRUMENTATION_FILTER_INTENSITY");
        if (intensity_env) {
            double intensity_percent = strtod(intensity_env, NULL);
            if (intensity_percent >= 0.0 && intensity_percent <= 100.0) {
                config->alpha = intensity_percent / 100.0;
                printf("ADC filter intensity set to: " ANSI_COLOR_YELLOW "%.1f%%" ANSI_COLOR_RESET "\n", intensity_percent);
            } else {
                fprintf(stderr, "Invalid INSTRUMENTATION_FILTER_INTENSITY value: " ANSI_COLOR_RED "%s" ANSI_COLOR_RESET " Using default " ANSI_COLOR_YELLOW "20.0%%" ANSI_COLOR_RESET ".\n", intensity_env);
                printf("ADC filter intensity set to default: " ANSI_COLOR_YELLOW "20.0%%" ANSI_COLOR_RESET "\n");
            }
        } else {
            printf("No ADC filter intensity set. Using default: " ANSI_COLOR_YELLOW "20.0%%" ANSI_COLOR_RESET "\n");
        }
    } else {
        printf("ADC filtering is " ANSI_COLOR_YELLOW "DISABLED" ANSI_COLOR_RESET ". Set INSTRUMENTATION_FILTER_ADC=1 to enable.\n");
    }
}

// getMeasurements now takes the pre-loaded filter configuration and is much more efficient.
void getMeasurements(int i2c_handle, const FilterConfig* filterConfig, const MeasurementSetting* settings, Measurement* measurements) {
    int16_t adc_val;

    for (int i = 0; i < NUM_CHANNELS; i++) {
        if (ads1115_read(i2c_handle, i, settings[i].gain_setting, &adc_val) == 0) {
            if (adc_val >= 0 && adc_val < 32768) {
                if (filterConfig->enabled) {
                    // Apply the simple low-pass filter using the pre-calculated alpha
                    measurements[i].adc_value = (int)(measurements[i].adc_value * (1.0 - filterConfig->alpha) + adc_val * filterConfig->alpha);
                } else {
                    // No filtering
                    measurements[i].adc_value = adc_val;
                }
            }
        } else {
            fprintf(stderr, "Failed to read from ADC channel %d\n", i);
        }
    }
}

void run_measurement_loop(int i2c_handle, const InfluxDBContext* dbContext, const FilterConfig* filterConfig, Measurement* measurements, MeasurementSetting* settings) {
    const char* send_to_db_env = getenv("INFLUXDB_SEND_DATA");
    bool send_to_db = (send_to_db_env != NULL && (strcmp(send_to_db_env, "1") == 0 || strcmp(send_to_db_env, "true") == 0));
    
    if (send_to_db) {
        printf(ANSI_COLOR_GREEN "Data will be sent to InfluxDB.\n" ANSI_COLOR_RESET);
    } else {
        printf(ANSI_COLOR_RED"Data will NOT be sent to InfluxDB. Set environment variable INFLUXDB_SEND_DATA=1 to enable.\n"ANSI_COLOR_RESET);
    }
    sleep(1); // Give time for the user to read initial output

    while (g_keep_running) {
        int requested_index = -1;

        pthread_mutex_lock(&calibration_mutex);
        requested_index = g_calibration_sensor_index;
        pthread_mutex_unlock(&calibration_mutex);

        if (requested_index != -1) {
            printf("--- Entering Calibration Mode for A%d ---\n", requested_index);
            double slope, offset;
            if (calibrateSensor(requested_index, measurements[requested_index].adc_value, &slope, &offset)) {
                printf("Calibration successful. Applying new settings for A%d.\n", requested_index);
                setMeasurementCorrection(&measurements[requested_index], slope, offset);
                // TODO: Save the new calibration to the config file.
            } else {
                printf("--- Calibration Aborted ---\n");
            }
            pthread_mutex_lock(&calibration_mutex);
            g_calibration_sensor_index = -1;
            pthread_mutex_unlock(&calibration_mutex);
            printf("--- Exiting Calibration Mode ---\n");
        }

        getMeasurements(i2c_handle, filterConfig, settings, measurements);
        printMeasurements(measurements, settings);

        if (send_to_db) {
            sendDataToInfluxDB(dbContext, measurements, settings);
        }
    }
}


void applyConfigurations(const MeasurementSetting* settings, Measurement* measurements) {
    for (int i = 0; i < NUM_CHANNELS; i++) {
        setDefaultMeasurement(&measurements[i]);
        setMeasurementId(&measurements[i], settings[i].id);
        setMeasurementCorrection(&measurements[i], settings[i].slope, settings[i].offset);
    }
}

// This function now dynamically calculates the width of the box and draws it.
void printConfigurations(const char* config_file_str, const MeasurementSetting* settings) {
    // --- Step 1: Find the maximum width needed for the 'Gain' column ---
    int max_gain_len = 0;
    for (int i = 0; i < NUM_CHANNELS; i++) {
        int len = strlen(settings[i].gain_setting);
        if (len > max_gain_len) {
            max_gain_len = len;
        }
    }

    // --- Step 2: Calculate the total width of the content line ---
    // This is the width *inside* the box borders, excluding the outer padding spaces.
    // Format: "[A0] ID: ... | Slope: ... | Offset: ... | Gain: ..."
    const int content_width = 9 + 30 + 10 + 10 + 11 + 10 + 9 + max_gain_len;
    const int box_inner_width = content_width + 2; // Add 2 for " " padding on left and right

    // --- Step 3: Print the box ---
    printf("\n");

    // Top border
    putchar('+');
    for (int i = 0; i < box_inner_width; i++) { putchar('-'); }
    printf("+\n");

    // Centered Title line
    char title_buffer[256];
    int title_len = snprintf(title_buffer, sizeof(title_buffer), "Configuration settings from board [%s]", config_file_str);
    if (title_len < box_inner_width) {
        int padding_total = box_inner_width - title_len;
        int padding_left = padding_total / 2;
        int padding_right = padding_total - padding_left;
        printf("|%*s" ANSI_COLOR_CYAN "%s" ANSI_COLOR_RESET "%*s|\n", padding_left, "", title_buffer, padding_right, "");
    } else { // If title is too long, just print it left-aligned and truncated
        printf("| " ANSI_COLOR_CYAN "%.*s" ANSI_COLOR_RESET " |\n", box_inner_width - 2, title_buffer);
    }
    
    // Separator line
    putchar('|');
    for (int i = 0; i < box_inner_width; i++) { putchar('-'); }
    printf("|\n");

    // Data lines
    for (int i = 0; i < NUM_CHANNELS; i++) {
        // The content is formatted to fit exactly within the content_width
        printf("| " ANSI_COLOR_YELLOW "[A%d] ID: %-30s | Slope: %-10.8f | Offset: %-10.6f | Gain: %-*s" ANSI_COLOR_RESET " |\n",
               i, settings[i].id, settings[i].slope, settings[i].offset, max_gain_len, settings[i].gain_setting);
    }

    // Bottom border
    putchar('+');
    for (int i = 0; i < box_inner_width; i++) { putchar('-'); }
    printf("+\n\n");
}

void printMeasurements(const Measurement* measurements, const MeasurementSetting* settings) {
    char time_buf[64];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%S%z", tm_info);

    printf("\n--- Sensor Readings at " ANSI_COLOR_CYAN "%s" ANSI_COLOR_RESET " ---\n", time_buf);
    for (int i = 0; i < NUM_CHANNELS; i++) {
        printf("A%d: " ANSI_COLOR_YELLOW "%-25s" ANSI_COLOR_RESET " | ADC: " ANSI_COLOR_MAGENTA "%5d" ANSI_COLOR_RESET " | Value: " ANSI_COLOR_GREEN "%8.2f" ANSI_COLOR_RESET " %s\n",
               i, settings[i].id, measurements[i].adc_value,
               getMeasurementValue(&measurements[i]), settings[i].unit);
    }
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <i2c-bus> <i2c-address-hex> <config-file>\n", argv[0]);
        return 1;
    }

    const char* i2c_bus_str = argv[1];
    const char* i2c_address_str = argv[2];
    const char* config_file_str = argv[3];

    printf("\n********************************************\n");
    printf(ANSI_COLOR_GREEN "Starting Instrumentation App\n" ANSI_COLOR_RESET);

    // --- Load InfluxDB configuration ---
    InfluxDBContext dbContext;
    const char* token = getenv("INFLUXDB_TOKEN");
    const char* org = getenv("INFLUXDB_ORG");
    const char* bucket = getenv("INFLUXDB_BUCKET");

    if (token == NULL) {
        fprintf(stderr, "Error: INFLUXDB_TOKEN environment variable not set.\n");
        return 1;
    }
    if (org == NULL) org = "Innoboat";
    if (bucket == NULL) bucket = "Innomaker";

    strncpy(dbContext.token, token, sizeof(dbContext.token) - 1);
    strncpy(dbContext.org, org, sizeof(dbContext.org) - 1);
    strncpy(dbContext.bucket, bucket, sizeof(dbContext.bucket) - 1);
    dbContext.token[sizeof(dbContext.token) - 1] = '\0';
    dbContext.org[sizeof(dbContext.org) - 1] = '\0';
    dbContext.bucket[sizeof(dbContext.bucket) - 1] = '\0';
    printf("InfluxDB Target: Org=" ANSI_COLOR_YELLOW "'%s'" ANSI_COLOR_RESET ", Bucket=" ANSI_COLOR_YELLOW "'%s'" ANSI_COLOR_RESET "\n", dbContext.org, dbContext.bucket);

    // --- Load Filter configuration ---
    FilterConfig filterConfig;
    loadFilterConfiguration(&filterConfig);

    // --- Initialize I2C ---
    long i2c_address = strtol(i2c_address_str, NULL, 16);
    if (i2c_address <= 0 || i2c_address == LONG_MAX || i2c_address == LONG_MIN) {
        fprintf(stderr, "Invalid I2C address provided: %s\n", i2c_address_str);
        return 1;
    }
    int i2c_handle = ads1115_init(i2c_bus_str, i2c_address);
    if (i2c_handle < 0) {
        return 1;
    }

    // --- Load Sensor configurations ---
    Measurement measurements[NUM_CHANNELS];
    MeasurementSetting settings[NUM_CHANNELS];
    loadConfigurationFile(config_file_str, settings);
    printConfigurations(config_file_str, settings);
    applyConfigurations(settings, measurements);

    // --- Start background threads and main loop ---
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    pthread_t calibration_listener_thread;
    CalibrationThreadArgs thread_args = {
        .sensor_index_ptr = &g_calibration_sensor_index,
        .mutex = &calibration_mutex,
        .keep_running_ptr = &g_keep_running
    };
    if (pthread_create(&calibration_listener_thread, NULL, calibrationListener, &thread_args) != 0) {
        perror("Failed to create calibration listener thread");
        ads1115_close(i2c_handle);
        return 1;
    }

    run_measurement_loop(i2c_handle, &dbContext, &filterConfig, measurements, settings);

    // --- Cleanup ---
    printf("Waiting for calibration listener thread to exit...\n");
    pthread_join(calibration_listener_thread, NULL);
    ads1115_close(i2c_handle);
    pthread_mutex_destroy(&calibration_mutex);
    printf("Shutdown complete.\n");

    return 0;
}
