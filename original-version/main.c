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
#include <stdbool.h> 

#include <gps.h>

#include "LineProtocol.h"
#include "CalibrationHelper.h"
#include "Measurement.h"
#include "util.h"
#include "ConfigurationLoader.h"
#include "ADS1115.h"
#include "ansi_colors.h"
#include "CsvLogger.h"
#include "OfflineQueue.h" // Include the new offline queue header

// --- Constants ---
#define MEASUREMENT_DELAY_US 100000
#define MAIN_LOOP_DELAY_US 1000 * 10
#define OFFLINE_QUEUE_PROCESS_INTERVAL_S 10

// --- Configuration Structs ---
typedef struct {
    bool enabled;
    double alpha;
} FilterConfig;

// --- Global variables ---
pthread_mutex_t calibration_mutex = PTHREAD_MUTEX_INITIALIZER;
int g_calibration_sensor_index = -1;
volatile sig_atomic_t g_keep_running = 1;
CsvLogger g_csv_logger;

// --- Function Prototypes ---
void signal_handler(int signum);
void loadFilterConfiguration(FilterConfig* config);
void run_measurement_loop(int i2c_handle, struct gps_data_t* gps_data, const InfluxDBContext* dbContext, const FilterConfig* filterConfig, Measurement* measurements, MeasurementSetting* settings);
void getMeasurements(int i2c_handle, const FilterConfig* filterConfig, const MeasurementSetting* settings, Measurement* measurements);
void getGPSData(struct gps_data_t* gps_data, GPSData* gps_measurements);
void applyConfigurations(const MeasurementSetting* settings, Measurement* measurements);
void printConfigurations(const char* config_file_str, const MeasurementSetting* settings);
void printMeasurements(const Measurement* measurements, const MeasurementSetting* settings, const GPSData* gps_data);
void* offline_queue_thread_func(void* arg); // Offline queue processing thread

// --- Implementations ---

void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        printf("\nTermination signal received. Shutting down...\n");
        g_keep_running = 0;
    }
}

void loadFilterConfiguration(FilterConfig* config) {
    config->enabled = false;
    config->alpha = 0.2;
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
            }
        }
    } else {
        printf("ADC filtering is " ANSI_COLOR_YELLOW "DISABLED" ANSI_COLOR_RESET ". Set INSTRUMENTATION_FILTER_ADC=1 to enable.\n");
    }
}

void getMeasurements(int i2c_handle, const FilterConfig* filterConfig, const MeasurementSetting* settings, Measurement* measurements) {
    int16_t adc_val;
    for (int i = 0; i < NUM_CHANNELS; i++) {
        if (ads1115_read(i2c_handle, i, settings[i].gain_setting, &adc_val) == 0) {
            if (adc_val >= 0 && adc_val < 32768) {
                if (filterConfig->enabled) {
                    measurements[i].adc_value = (int)(measurements[i].adc_value * (1.0 - filterConfig->alpha) + adc_val * filterConfig->alpha);
                } else {
                    measurements[i].adc_value = adc_val;
                }
            }
        } else {
            fprintf(stderr, "Failed to read from ADC channel %d\n", i);
        }
    }
}

void getGPSData(struct gps_data_t* gps_data, GPSData* gps_measurements) {
    if (gps_waiting(gps_data, 1000)) {
        if (gps_read(gps_data, NULL, 0) == -1) {
            fprintf(stderr, "GPS read error.\n");
            return;
        }
        if ((gps_data->set & LATLON_SET) != 0) {
            gps_measurements->latitude = gps_data->fix.latitude;
            gps_measurements->longitude = gps_data->fix.longitude;
        }
        if ((gps_data->set & ALTITUDE_SET) != 0) {
            gps_measurements->altitude = gps_data->fix.altitude;
        }
        if ((gps_data->set & SPEED_SET) != 0) {
            gps_measurements->speed = gps_data->fix.speed;
        }
    }
}

void run_measurement_loop(int i2c_handle, struct gps_data_t* gps_data, const InfluxDBContext* dbContext, const FilterConfig* filterConfig, Measurement* measurements, MeasurementSetting* settings) {
    const char* send_to_db_env = getenv("INFLUXDB_SEND_DATA");
    bool send_to_db = (send_to_db_env != NULL && (strcmp(send_to_db_env, "1") == 0 || strcmp(send_to_db_env, "true") == 0));
    
    GPSData gps_measurements = { .latitude = NAN, .longitude = NAN, .altitude = NAN, .speed = NAN };

    if (send_to_db) {
        printf(ANSI_COLOR_GREEN "Data will be sent to InfluxDB.\n" ANSI_COLOR_RESET);
    } else {
        printf(ANSI_COLOR_RED"Data will NOT be sent to InfluxDB. Set environment variable INFLUXDB_SEND_DATA=1 to enable.\n"ANSI_COLOR_RESET);
    }
    sleep(1);

    struct timespec last_send_time;
    clock_gettime(CLOCK_MONOTONIC, &last_send_time);

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
            } else {
                printf("--- Calibration Aborted ---\n");
            }
            pthread_mutex_lock(&calibration_mutex);
            g_calibration_sensor_index = -1;
            pthread_mutex_unlock(&calibration_mutex);
            printf("--- Exiting Calibration Mode ---\n");
        }

        getGPSData(gps_data, &gps_measurements);
        getMeasurements(i2c_handle, filterConfig, settings, measurements);
        
        printMeasurements(measurements, settings, &gps_measurements);

        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        double time_diff = (current_time.tv_sec - last_send_time.tv_sec) + (current_time.tv_nsec - last_send_time.tv_nsec) / 1e9;

        #define INFLUXDB_SEND_INTERVAL_S 0.5
        if (send_to_db && (time_diff >= INFLUXDB_SEND_INTERVAL_S)) {
            last_send_time = current_time;
            sendDataToInfluxDB(dbContext, measurements, settings, &gps_measurements);
        }

        csv_logger_log(&g_csv_logger, measurements, &gps_measurements);
        
        usleep(MAIN_LOOP_DELAY_US);
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
    int max_gain_len = 0;
    for (int i = 0; i < NUM_CHANNELS; i++) {
        int len = strlen(settings[i].gain_setting);
        if (len > max_gain_len) max_gain_len = len;
    }
    const int content_width = 9 + 30 + 10 + 10 + 11 + 10 + 9 + max_gain_len;
    const int box_inner_width = content_width + 2;
    printf("\n");
    putchar('+');
    for (int i = 0; i < box_inner_width; i++) { putchar('-'); }
    printf("+\n");
    char title_buffer[256];
    int title_len = snprintf(title_buffer, sizeof(title_buffer), "Configuration settings from board [%s]", config_file_str);
    if (title_len < box_inner_width) {
        int padding_total = box_inner_width - title_len;
        int padding_left = padding_total / 2;
        int padding_right = padding_total - padding_left;
        printf("|%*s" ANSI_COLOR_CYAN "%s" ANSI_COLOR_RESET "%*s|\n", padding_left, "", title_buffer, padding_right, "");
    } else { 
        printf("| " ANSI_COLOR_CYAN "%.*s" ANSI_COLOR_RESET " |\n", box_inner_width - 2, title_buffer);
    }
    putchar('|');
    for (int i = 0; i < box_inner_width; i++) { putchar('-'); }
    printf("|\n");
    for (int i = 0; i < NUM_CHANNELS; i++) {
        printf("| " ANSI_COLOR_YELLOW "[A%d] ID: %-30s | Slope: %-10.8f | Offset: %-10.6f | Gain: %-*s" ANSI_COLOR_RESET " |\n",
               i, settings[i].id, settings[i].slope, settings[i].offset, max_gain_len, settings[i].gain_setting);
    }
    putchar('+');
    for (int i = 0; i < box_inner_width; i++) { putchar('-'); }
    printf("+\n\n");
}

void printMeasurements(const Measurement* measurements, const MeasurementSetting* settings, const GPSData* gps_data) {
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

    printf("--- GPS Data ---\n");
    if (gps_data && !isnan(gps_data->latitude)) {
        printf("Lat: " ANSI_COLOR_CYAN "%.6f" ANSI_COLOR_RESET ", Lon: " ANSI_COLOR_CYAN "%.6f" ANSI_COLOR_RESET "\n", gps_data->latitude, gps_data->longitude);
        printf("Alt: " ANSI_COLOR_CYAN "%.2f m" ANSI_COLOR_RESET ", Speed: " ANSI_COLOR_CYAN "%.2f m/s" ANSI_COLOR_RESET "\n", gps_data->altitude, gps_data->speed);
    } else {
        printf(ANSI_COLOR_RED "No GPS fix available.\n" ANSI_COLOR_RESET);
    }
}

void* offline_queue_thread_func(void* arg) {
    const InfluxDBContext* dbContext = (const InfluxDBContext*)arg;
    int elapsed_time = 0;
    while (g_keep_running) {
        if (!g_keep_running) break; // Exit immediately if shutdown is requested
        elapsed_time++;
        if (elapsed_time >= OFFLINE_QUEUE_PROCESS_INTERVAL_S) {
            offline_queue_process(dbContext);
            elapsed_time = 0; // Reset timer
        }
        sleep(1);
    }
    printf("Offline queue processor shutting down.\n");
    return NULL;
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
    if (!token) {
        fprintf(stderr, "Error: INFLUXDB_TOKEN environment variable not set.\n");
        return 1;
    }
    if (!org) org = "Innoboat";
    if (!bucket) bucket = "Innomaker";
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
    if (i2c_handle < 0) return 1;

    // --- Initialize GPS ---
    struct gps_data_t gps_data;
    if (gps_open("localhost", "2947", &gps_data) != 0) {
        perror("GPS: Could not connect to gpsd");
        ads1115_close(i2c_handle);
        return 1;
    }
    (void)gps_stream(&gps_data, WATCH_ENABLE | WATCH_JSON, NULL);
    printf(ANSI_COLOR_GREEN "Successfully connected to gpsd.\n" ANSI_COLOR_RESET);

    // --- Load Sensor configurations ---
    Measurement measurements[NUM_CHANNELS];
    MeasurementSetting settings[NUM_CHANNELS];
    loadConfigurationFile(config_file_str, settings);
    printConfigurations(config_file_str, settings);
    applyConfigurations(settings, measurements);

    // --- Initialize CSV Logger ---
    csv_logger_init(&g_csv_logger, settings);
    
    // --- Initialize Offline Queue ---
    offline_queue_init();

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
        (void)gps_close(&gps_data);
        csv_logger_close(&g_csv_logger);
        return 1;
    }
    
    pthread_t offline_queue_thread;
    if (pthread_create(&offline_queue_thread, NULL, offline_queue_thread_func, &dbContext) != 0) {
        perror("Failed to create offline queue processing thread");
        g_keep_running = 0; // Signal other threads to stop
        pthread_join(calibration_listener_thread, NULL);
        ads1115_close(i2c_handle);
        (void)gps_close(&gps_data);
        csv_logger_close(&g_csv_logger);
        return 1;
    }

    run_measurement_loop(i2c_handle, &gps_data, &dbContext, &filterConfig, measurements, settings);

    // --- Cleanup ---
    printf("Waiting for background threads to exit...\n");
    pthread_join(calibration_listener_thread, NULL);
    pthread_join(offline_queue_thread, NULL);
    
    (void)gps_stream(&gps_data, WATCH_DISABLE, NULL);
    (void)gps_close(&gps_data);
    
    ads1115_close(i2c_handle);
    pthread_mutex_destroy(&calibration_mutex);

    csv_logger_close(&g_csv_logger);

    printf("Shutdown complete.\n");

    return 0;
}
