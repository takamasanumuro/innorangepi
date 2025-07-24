#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <pthread.h>
#include <signal.h> // For signal handling
#include <limits.h> // For LONG_MAX, LONG_MIN

#include "LineProtocol.h"
#include "CalibrationHelper.h"
#include "Measurement.h"
#include "util.h"
#include "ConfigurationLoader.h" // Added to fix implicit declaration warning

// Include the curl library for HTTP requests
#ifdef __aarch64__
#include </usr/include/aarch64-linux-gnu/curl/curl.h>
#elif __x86_64__
#include </usr/include/x86_64-linux-gnu/curl/curl.h>
#elif __arm__
#include </usr/include/arm-linux-gnueabihf/curl/curl.h>
#else
#include <curl/curl.h>
#endif

// --- Constants ---
#define RATE_128 4
#define MEASUREMENT_DELAY_US 100000 // Delay between readings on different channels
#define MAIN_LOOP_DELAY_MS 100
#define INFLUXDB_IP "144.22.131.217"
#define INFLUXDB_PORT 8086

// ADC gain settings
#define GAIN_6144MV 0
#define GAIN_4096MV 1
#define GAIN_2048MV 2
#define GAIN_1024MV 3
#define GAIN_512MV  4
#define GAIN_256MV  5

// ADC multiplexer settings
#define AIN0 4
#define AIN1 5
#define AIN2 6
#define AIN3 7

// ADC register addresses
#define REG_CONV 0
#define REG_CONFIG 1

// --- Global variables for concurrency ---
pthread_mutex_t calibration_mutex = PTHREAD_MUTEX_INITIALIZER;
int g_calibration_sensor_index = -1; // Global index for requested calibration
volatile sig_atomic_t g_keep_running = 1; // Global flag to control main loop

// --- Function Prototypes ---
void signal_handler(int signum);
int gain_to_int(const char* gain_str);
int16_t readAdc(int i2c_handle, uint8_t multiplexer, uint8_t gain, int16_t *conversionResult);
void sendMeasurementToInfluxDB(const InfluxDBContext* dbContext, Measurement* measurements, MeasurementSetting* settings);
void run_measurement_loop(int i2c_handle, const InfluxDBContext* dbContext, Measurement* measurements, MeasurementSetting* settings);
void getMeasurements(int i2c_handle, const MeasurementSetting* settings, Measurement* measurements);
void applyConfigurations(const MeasurementSetting* settings, Measurement* measurements);
void printConfigurations(const char* config_file_str, const MeasurementSetting* settings);
void printMeasurements(const Measurement* measurements, const MeasurementSetting* settings);

// --- Implementations ---

void signal_handler(int signum) {
    // This handler will catch Ctrl+C (SIGINT) and gracefully shut down the application.
    if (signum == SIGINT || signum == SIGTERM) {
        printf("\nTermination signal received. Shutting down...\n");
        g_keep_running = 0;
    }
}

int gain_to_int(const char* gain_str) {
    if (strcmp(gain_str, "GAIN_6144MV") == 0) return GAIN_6144MV;
    if (strcmp(gain_str, "GAIN_4096MV") == 0) return GAIN_4096MV;
    if (strcmp(gain_str, "GAIN_2048MV") == 0) return GAIN_2048MV;
    if (strcmp(gain_str, "GAIN_1024MV") == 0) return GAIN_1024MV;
    if (strcmp(gain_str, "GAIN_512MV") == 0) return GAIN_512MV;
    if (strcmp(gain_str, "GAIN_256MV") == 0) return GAIN_256MV;
    return -1; // Invalid gain string
}

int16_t readAdc(int i2c_handle, uint8_t multiplexer, uint8_t gain, int16_t *conversionResult) {
    unsigned char config[3];
    config[0] = REG_CONFIG;
    config[1] = (multiplexer << 4) | (gain << 1) | 0x81; // Set OS bit to start conversion
    config[2] = (RATE_128 << 5) | 3; // Set rate and disable comparator

    if (write(i2c_handle, config, 3) != 3) return -1;

    // Wait for the conversion to complete by polling the OS bit.
    // The ADS1115 datasheet says conversion time for 128SPS is ~7.8ms.
    // A 100ms sleep is more than enough and simpler than a tight poll loop.
    usleep(10000); // Wait 10ms

    // Point to the conversion register
    config[0] = REG_CONV;
    if (write(i2c_handle, &config[0], 1) != 1) return -4;

    // Read the 2-byte result
    unsigned char read_buf[2];
    if (read(i2c_handle, read_buf, 2) != 2) return -5;

    *conversionResult = (int16_t)((read_buf[0] << 8) | read_buf[1]);
    return 0;
}

void CurlInfluxDB(const InfluxDBContext* dbContext, const char* lineProtocol) {
    CURL* curl_handle = curl_easy_init();
    if (!curl_handle) {
        fprintf(stderr, "Failed to initialize CURL\n");
        return;
    }

    struct MemoryStruct chunk = { .memory = malloc(1), .size = 0 };
    if (!chunk.memory) {
        fprintf(stderr, "Failed to allocate memory for CURL response\n");
        curl_easy_cleanup(curl_handle);
        return;
    }

    char url[256];
    snprintf(url, sizeof(url), "http://%s:%d/api/v2/write?org=%s&bucket=%s&precision=s",
             INFLUXDB_IP, INFLUXDB_PORT, dbContext->org, dbContext->bucket);

    // Increased buffer size to prevent format truncation warning.
    // The prefix "Authorization: Token " is 21 chars. The token can be up to 256.
    // 21 + 256 + 1 (null terminator) = 278. 512 is a safe size.
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Token %s", dbContext->token);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: text/plain; charset=utf-8");

    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, lineProtocol);

    CURLcode result = curl_easy_perform(curl_handle);
    if (result != CURLE_OK) {
        fprintf(stderr, "CURL error: %s\n", curl_easy_strerror(result));
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl_handle);
    free(chunk.memory);
}

void sendMeasurementToInfluxDB(const InfluxDBContext* dbContext, Measurement* measurements, MeasurementSetting* settings) {
    char line_protocol_data[512];
    setMeasurement(line_protocol_data, sizeof(line_protocol_data), "measurements");
    addTag(line_protocol_data, sizeof(line_protocol_data), "source", "instrumentacao");

    for (int i = 0; i < NUM_CHANNELS; i++) {
        if (strcmp(settings[i].id, "NC") != 0) {
            addField(line_protocol_data, sizeof(line_protocol_data), settings[i].id, getMeasurementValue(&measurements[i]));
        }
    }
    addTimestamp(line_protocol_data, sizeof(line_protocol_data), getEpochSeconds());

    CurlInfluxDB(dbContext, line_protocol_data);
}

void getMeasurements(int i2c_handle, const MeasurementSetting* settings, Measurement* measurements) {
    int16_t adc_val;
    int gains[] = {
        gain_to_int(settings[0].gain_setting),
        gain_to_int(settings[1].gain_setting),
        gain_to_int(settings[2].gain_setting),
        gain_to_int(settings[3].gain_setting)
    };
    uint8_t channels[] = {AIN0, AIN1, AIN2, AIN3};

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
        sendMeasurementToInfluxDB(dbContext, measurements, settings);

        usleep(MAIN_LOOP_DELAY_MS * 1000);
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
               getMeasurementValue((Measurement*)&measurements[i]), // getMeasurementValue is not const-correct
               settings[i].unit);
    }
    printf("\n");
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <i2c-bus> <i2c-address-hex> <config-file>\n", argv[0]);
        fprintf(stderr, "Example: %s /dev/i2c-1 0x48 configA\n", argv[0]);
        return 1;
    }

    const char* i2c_bus_str = argv[1];
    const char* i2c_address_str = argv[2];
    const char* config_file_str = argv[3];

    printf("\n********************************************\n");
    printf("Starting Instrumentation App\n");

    // Get InfluxDB token from environment variable for security
    const char* token = getenv("INFLUXDB_TOKEN");
    if (token == NULL) {
        fprintf(stderr, "Error: INFLUXDB_TOKEN environment variable not set.\n");
        return 1;
    }
    InfluxDBContext dbContext;
    strncpy(dbContext.token, token, sizeof(dbContext.token) - 1);
    dbContext.token[sizeof(dbContext.token) - 1] = '\0';
    strncpy(dbContext.org, "Innoboat", sizeof(dbContext.org) - 1);
    strncpy(dbContext.bucket, "Innomaker", sizeof(dbContext.bucket) - 1);

    long i2c_address = strtol(i2c_address_str, NULL, 16);
    if (i2c_address <= 0 || i2c_address == LONG_MAX || i2c_address == LONG_MIN) {
        fprintf(stderr, "Invalid I2C address provided: %s\n", i2c_address_str);
        return 1;
    }

    int i2c_handle = open(i2c_bus_str, O_RDWR);
    if (i2c_handle < 0) {
        perror("Error opening I2C bus");
        return 1;
    }

    if (ioctl(i2c_handle, I2C_SLAVE, i2c_address) < 0) {
        perror("Error setting I2C slave address. Check device connection and address");
        close(i2c_handle);
        return 1;
    }
    printf("Successfully connected to I2C device at 0x%lX on %s\n", i2c_address, i2c_bus_str);

    Measurement measurements[NUM_CHANNELS];
    MeasurementSetting settings[NUM_CHANNELS];
    
    loadConfigurationFile(config_file_str, settings);
    printConfigurations(config_file_str, settings);
    applyConfigurations(settings, measurements);

    // Set up signal handler for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Set up and start the calibration listener thread
    pthread_t calibration_listener_thread;
    CalibrationThreadArgs thread_args = {
        .sensor_index_ptr = &g_calibration_sensor_index,
        .mutex = &calibration_mutex,
        .keep_running_ptr = &g_keep_running
    };
    if (pthread_create(&calibration_listener_thread, NULL, calibrationListener, &thread_args) != 0) {
        perror("Failed to create calibration listener thread");
        close(i2c_handle);
        return 1;
    }

    // Start the main application logic
    run_measurement_loop(i2c_handle, &dbContext, measurements, settings);

    // Clean up
    printf("Waiting for calibration listener thread to exit...\n");
    pthread_join(calibration_listener_thread, NULL);
    close(i2c_handle);
    pthread_mutex_destroy(&calibration_mutex);
    printf("Shutdown complete.\n");

    return 0;
}
