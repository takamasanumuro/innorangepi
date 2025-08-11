#include "ApplicationManager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <math.h>

// All required headers from the original main.c
#include "ADS1115.h"
#include "ansi_colors.h"
#include "BatteryMonitor.h"
#include "CalibrationHelper.h"
#include "ConfigurationLoader.h"
#include "CsvLogger.h"
#include "Measurement.h"
#include "Sender.h"
#include "SocketServer.h"
#include "util.h"
#include "DataPublisher.h"
#include "MeasurementCoordinator.h"
#include "TimingUtils.h"
#include "HardwareManager.h"

// The internal structure of the ApplicationManager, formerly AppContext
struct ApplicationManager {
    volatile sig_atomic_t keep_running;
    char i2c_bus_path[APP_I2C_BUS_PATH_MAX];
    long i2c_address;
    char config_file_path[APP_CONFIG_FILE_PATH_MAX];

    Channel channels[NUM_CHANNELS];
    GPSData gps_measurements;
    BatteryState battery_state;
    SenderContext* sender_ctx;
    CsvLogger csv_logger;
    
    pthread_mutex_t cal_mutex;
    int cal_sensor_index;
    
    HardwareManager hardware_manager;
    MeasurementCoordinator measurement_coordinator;
    DataPublisher* data_publisher;
    IntervalTimer send_timer;
};

// --- Private Function Prototypes ---
static void print_current_measurements(const Channel channels[], const GPSData* gps_data);

// --- Public API Implementation ---

ApplicationManager* app_manager_create(const char* i2c_bus, long i2c_address, const char* config_file) {
    // Input validation
    if (!i2c_bus || !config_file) {
        fprintf(stderr, "Invalid input parameters to app_manager_create\n");
        return NULL;
    }
    
    if (strlen(i2c_bus) >= APP_I2C_BUS_PATH_MAX) {
        fprintf(stderr, "I2C bus path too long: %s\n", i2c_bus);
        return NULL;
    }
    
    if (strlen(config_file) >= APP_CONFIG_FILE_PATH_MAX) {
        fprintf(stderr, "Config file path too long: %s\n", config_file);
        return NULL;
    }

    ApplicationManager* app = calloc(1, sizeof(ApplicationManager));
    if (!app) {
        perror("Failed to allocate memory for ApplicationManager");
        return NULL;
    }

    app->keep_running = true;
    app->i2c_address = i2c_address;
    
    // Safe string copying with guaranteed null termination
    strncpy(app->i2c_bus_path, i2c_bus, sizeof(app->i2c_bus_path) - 1);
    app->i2c_bus_path[sizeof(app->i2c_bus_path) - 1] = '\0';
    
    strncpy(app->config_file_path, config_file, sizeof(app->config_file_path) - 1);
    app->config_file_path[sizeof(app->config_file_path) - 1] = '\0';

    return app;
}

AppManagerError app_manager_init(ApplicationManager* app) {
    if (!app) {
        return APP_ERROR_NULL_POINTER;
    }

    // 1. Initialize hardware first
    if (!hardware_manager_init(&app->hardware_manager, app->i2c_bus_path, app->i2c_address)) {
        fprintf(stderr, "Hardware manager initialization failed\n");
        return APP_ERROR_HARDWARE_INIT_FAILED;
    }

    // 2. Initialize mutex with error checking
    int mutex_result = pthread_mutex_init(&app->cal_mutex, NULL);
    if (mutex_result != 0) {
        fprintf(stderr, "Failed to initialize mutex: %d\n", mutex_result);
        hardware_manager_cleanup(&app->hardware_manager);
        return APP_ERROR_MUTEX_INIT_FAILED;
    }
    
    // Load configurations for sensors
    if (!loadConfigurationFile(app->config_file_path, app->channels)) {
        fprintf(stderr, "Configuration file load failed\n");
        pthread_mutex_destroy(&app->cal_mutex);
        hardware_manager_cleanup(&app->hardware_manager);
        return APP_ERROR_CONFIG_LOAD_FAILED;
    }
    
    // Initialize sender
    app->sender_ctx = sender_create_from_env();
    if (!app->sender_ctx) {
        fprintf(stderr, "Sender initialization failed\n");
        pthread_mutex_destroy(&app->cal_mutex);
        hardware_manager_cleanup(&app->hardware_manager);
        return APP_ERROR_SENDER_INIT_FAILED;
    }
    
    // Initialize channels (some properties are set from config, some are runtime)
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        // Mark channel as active if it has a valid ID from the config file
        if (strncmp(app->channels[i].id, "NC", MEASUREMENT_ID_SIZE) != 0 && strlen(app->channels[i].id) > 0) {
            app->channels[i].is_active = true;
        } else {
            app->channels[i].is_active = false;
        }
    }
    
    // Initialize high-level coordinators
    if (!measurement_coordinator_init(&app->measurement_coordinator,
                                     hardware_manager_get_i2c_handle(&app->hardware_manager),
                                     hardware_manager_get_gps_data(&app->hardware_manager),
                                     app->channels,
                                     &app->gps_measurements)) {
        fprintf(stderr, "Failed to initialize Measurement Coordinator.\n");
        sender_destroy(app->sender_ctx);
        pthread_mutex_destroy(&app->cal_mutex);
        hardware_manager_cleanup(&app->hardware_manager);
        return APP_ERROR_COORDINATOR_INIT_FAILED;
    }
    
    app->data_publisher = data_publisher_create(app->sender_ctx);
    if (!app->data_publisher) {
        fprintf(stderr, "Failed to create Data Publisher.\n");
        sender_destroy(app->sender_ctx);
        pthread_mutex_destroy(&app->cal_mutex);
        hardware_manager_cleanup(&app->hardware_manager);
        return APP_ERROR_PUBLISHER_INIT_FAILED;
    }
    
    interval_timer_init(&app->send_timer, APP_INFLUXDB_SEND_INTERVAL_S);
    csv_logger_init(&app->csv_logger, app->channels);
    
    // Initialize battery monitor
    battery_monitor_init(&app->battery_state, app->channels);

    printf("Application Manager initialized successfully.\n");
    return APP_SUCCESS;
}

void app_manager_run(ApplicationManager* app) {
    if (!app) return;

    while (app->keep_running) {
        measurement_coordinator_collect(&app->measurement_coordinator);
        
        if (interval_timer_should_trigger(&app->send_timer)) {
            data_publisher_publish(app->data_publisher, app->channels, &app->gps_measurements);
            interval_timer_mark_triggered(&app->send_timer);
        }
        
        csv_logger_log(&app->csv_logger, app->channels, &app->gps_measurements);
        print_current_measurements(app->channels, &app->gps_measurements);
        
        usleep(APP_MAIN_LOOP_DELAY_US);
    }
}

void app_manager_destroy(ApplicationManager* app) {
    if (!app) return;

    printf("\nCleaning up resources...\n");
    
    data_publisher_destroy(app->data_publisher);
    hardware_manager_cleanup(&app->hardware_manager);
    sender_destroy(app->sender_ctx);
    csv_logger_close(&app->csv_logger);
    pthread_mutex_destroy(&app->cal_mutex);
    
    free(app);
}

void app_manager_signal_shutdown(ApplicationManager* app) {
    if (!app) return;
    const char msg[] = "\nTermination signal received. Shutting down...\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    app->keep_running = false;
}

const char* app_manager_error_string(AppManagerError error) {
    switch (error) {
        case APP_SUCCESS:
            return "Success";
        case APP_ERROR_NULL_POINTER:
            return "Null pointer provided";
        case APP_ERROR_MEMORY_ALLOCATION:
            return "Memory allocation failed";
        case APP_ERROR_INVALID_PARAMETER:
            return "Invalid parameter";
        case APP_ERROR_HARDWARE_INIT_FAILED:
            return "Hardware initialization failed";
        case APP_ERROR_CONFIG_LOAD_FAILED:
            return "Configuration file load failed";
        case APP_ERROR_SENDER_INIT_FAILED:
            return "Sender initialization failed";
        case APP_ERROR_COORDINATOR_INIT_FAILED:
            return "Measurement coordinator initialization failed";
        case APP_ERROR_PUBLISHER_INIT_FAILED:
            return "Data publisher initialization failed";
        case APP_ERROR_MUTEX_INIT_FAILED:
            return "Mutex initialization failed";
        default:
            return "Unknown error";
    }
}

// --- Private Helper Functions ---

static void print_current_measurements(const Channel channels[], const GPSData* gps_data) {
    // Simple placeholder. A more advanced implementation would format this nicely.
    printf("--- Current Measurements ---\n");
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        if (channels[i].is_active) {
            printf("  Channel %d (%s): ADC=%d, Value=%.4f %s\n",
                   i,
                   channels[i].id,
                   channels[i].raw_adc_value,
                   channel_get_calibrated_value(&channels[i]),
                   channels[i].unit);
        }
    }
    if (!isnan(gps_data->latitude) && !isnan(gps_data->longitude)) {
        printf("  GPS: Lat=%.6f, Lon=%.6f, Speed=%.2f kph\n",
               gps_data->latitude,
               gps_data->longitude,
               gps_data->speed);
    } else {
        printf("  GPS: No valid data\n");
    }
    printf("--------------------------\n");
}
