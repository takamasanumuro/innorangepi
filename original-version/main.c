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

#include "LineProtocol.h"
#include "CalibrationHelper.h"
#include "Measurement.h"
#include "util.h"
#include "ConfigurationLoader.h"
#include "ADS1115.h" // Include the new I2C device header

// --- Constants ---
#define MEASUREMENT_DELAY_US 100000
#define MAIN_LOOP_DELAY_S 1

// --- Global variables for concurrency ---
pthread_mutex_t calibration_mutex = PTHREAD_MUTEX_INITIALIZER;
int g_calibration_sensor_index = -1; // Global index for requested calibration
volatile sig_atomic_t g_keep_running = 1; // Global flag to control main loop

// --- Function Prototypes ---
void signal_handler(int signum);
void run_measurement_loop(int i2c_handle, const InfluxDBContext* dbContext, Measurement* measurements, MeasurementSetting* settings);
void getMeasurements(int i2c_handle, const MeasurementSetting* settings, Measurement* measurements);
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

void getMeasurements(int i2c_handle, const MeasurementSetting* settings, Measurement* measurements) {
    int16_t adc_val;

    for (int i = 0; i < NUM_CHANNELS; i++) {
        if (readAdc(i2c_handle, channels[i], gains[i], &adc_val) == 0) {
            // The ADC is 16-bit, but in single-ended mode, negative values are not expected.
            // A value > 32767 indicates an issue, often with wiring.
            // We keep the last valid reading in such cases.
            if (adc_val >= 0 && adc_val < 32768) {
                measurements[i].adc_value = adc_val;
            }
        } else {
            fprintf(stderr, "Failed to read from ADC channel %d\n", i);
        }
        usleep(MEASUREMENT_DELAY_US);
    }
}

void run_measurement_loop(int i2c_handle, const InfluxDBContext* dbContext, Measurement* measurements, MeasurementSetting* settings) {
    while (g_keep_running) {
        int requested_index = -1;

        // Safely check if a calibration has been requested.
        pthread_mutex_lock(&calibration_mutex);
        requested_index = g_calibration_sensor_index;
        pthread_mutex_unlock(&calibration_mutex);

        if (requested_index != -1) {
            printf("--- Entering Calibration Mode for A%d ---\n", requested_index);
            double slope, offset;
            // The calibrateSensor function is blocking and handles user input.
            if (calibrateSensor(requested_index, measurements[requested_index].adc_value, &slope, &offset)) {
                printf("Calibration successful. Applying new settings for A%d.\n", requested_index);
                setMeasurementCorrection(&measurements[requested_index], slope, offset);
                // TODO: Save the new calibration to the config file.
            } else {
                printf("--- Calibration Aborted ---\n");
            }

            // Safely reset the calibration request index.
            pthread_mutex_lock(&calibration_mutex);
            g_calibration_sensor_index = -1;
            pthread_mutex_unlock(&calibration_mutex);
            printf("--- Exiting Calibration Mode ---\n");
        }

        getMeasurements(i2c_handle, settings, measurements);
        printMeasurements(measurements, settings);
        sendDataToInfluxDB(dbContext, measurements, settings);

        usleep(20 * 1000);
    }
}


void applyConfigurations(const MeasurementSetting* settings, Measurement* measurements) {
    for (int i = 0; i < NUM_CHANNELS; i++) {
        setDefaultMeasurement(&measurements[i]);
        setMeasurementId(&measurements[i], settings[i].id);
        setMeasurementCorrection(&measurements[i], settings[i].slope, settings[i].offset);
    }
}

void printConfigurations(const char* config_file_str, const MeasurementSetting* settings) {
    printf("Configuration settings from board [%s]\n", config_file_str);
    for (int i = 0; i < NUM_CHANNELS; i++) {
        printf("[A%d] ID: %-25s | Slope: %-10.6f | Offset: %-10.6f | Gain: %s\n",
               i, settings[i].id, settings[i].slope, settings[i].offset, settings[i].gain_setting);
    }
    printf("\n");
}

void printMeasurements(const Measurement* measurements, const MeasurementSetting* settings) {

    for (int i = 0; i < NUM_CHANNELS; i++) {
        printf("A%d: %-25s | ADC: %5d | Value: %8.2f %s\n",
               i, settings[i].id, measurements[i].adc_value,
               getMeasurementValue(&measurements[i]),
               settings[i].unit);
    }
    printf("\n");
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
    printf("Starting Instrumentation App\n");

    // --- Get InfluxDB configuration from environment variables ---
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

    printf("InfluxDB Target: Org='%s', Bucket='%s'\n", dbContext.org, dbContext.bucket);

    long i2c_address = strtol(i2c_address_str, NULL, 16);
    if (i2c_address <= 0 || i2c_address == LONG_MAX || i2c_address == LONG_MIN) {
        fprintf(stderr, "Invalid I2C address provided: %s\n", i2c_address_str);
        return 1;
    }

    // Initialize I2C using the new dedicated function
    int i2c_handle = ads1115_init(i2c_bus_str, i2c_address);
    if (i2c_handle < 0) {
        return 1; // Error message is printed inside ads1115_init
    }

    Measurement measurements[NUM_CHANNELS];
    MeasurementSetting settings[NUM_CHANNELS];
    
    loadConfigurationFile(config_file_str, settings);
    printConfigurations(config_file_str, settings);
    applyConfigurations(settings, measurements);

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

    // Start the main application logic
    run_measurement_loop(i2c_handle, &dbContext, measurements, settings);

    // Clean up
    printf("Waiting for calibration listener thread to exit...\n");
    pthread_join(calibration_listener_thread, NULL);
    
    // Close the I2C handle using the dedicated function
    ads1115_close(i2c_handle);
    
    pthread_mutex_destroy(&calibration_mutex);
    printf("Shutdown complete.\n");

    return 0;
}
