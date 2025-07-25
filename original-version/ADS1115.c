#include "ADS1115.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>

// --- Internal Constants ---

// RATE in SPS (samples per second). Using a fixed rate for this application.
#define RATE_128 4

// GAIN in mV, max expected voltage as input
#define GAIN_6144MV 0
#define GAIN_4096MV 1
#define GAIN_2048MV 2
#define GAIN_1024MV 3
#define GAIN_512MV  4
#define GAIN_256MV  5

// Multiplexer settings for single-ended inputs
#define AIN0 4
#define AIN1 5
#define AIN2 6
#define AIN3 7

// Register addresses
#define REG_CONV 0
#define REG_CONFIG 1

// --- Internal Helper Functions ---

// Converts a gain setting string to its corresponding integer code for the ADC.
// This function is 'static' because it's only used within this file.
static int gain_to_int(const char* gain_str) {
    if (strcmp(gain_str, "GAIN_6144MV") == 0) return GAIN_6144MV;
    if (strcmp(gain_str, "GAIN_4096MV") == 0) return GAIN_4096MV;
    if (strcmp(gain_str, "GAIN_2048MV") == 0) return GAIN_2048MV;
    if (strcmp(gain_str, "GAIN_1024MV") == 0) return GAIN_1024MV;
    if (strcmp(gain_str, "GAIN_512MV") == 0) return GAIN_512MV;
    if (strcmp(gain_str, "GAIN_256MV") == 0) return GAIN_256MV;
    return -1; // Invalid gain string
}

// Maps a channel number (0-3) to the ADS1115's internal multiplexer setting.
static uint8_t channel_to_mux(uint8_t channel) {
    switch (channel) {
        case 0: return AIN0;
        case 1: return AIN1;
        case 2: return AIN2;
        case 3: return AIN3;
        default: return AIN0; // Default to AIN0 for safety
    }
}

// --- Public API Functions ---

int ads1115_init(const char* i2c_bus_str, long i2c_address) {
    int i2c_handle = open(i2c_bus_str, O_RDWR);
    if (i2c_handle < 0) {
        perror("ADS1115: Error opening I2C bus");
        return -1;
    }

    if (ioctl(i2c_handle, I2C_SLAVE, i2c_address) < 0) {
        perror("ADS1115: Error setting I2C slave address");
        close(i2c_handle);
        return -1;
    }

    printf("Successfully connected to ADS1115 at 0x%lX on %s\n", i2c_address, i2c_bus_str);
    return i2c_handle;
}

int ads1115_read(int i2c_handle, uint8_t channel, const char* gain_str, int16_t *conversionResult) {
    int gain = gain_to_int(gain_str);
    if (gain == -1) {
        fprintf(stderr, "ADS1115: Invalid gain setting '%s' for channel %d\n", gain_str, channel);
        return -1;
    }

    uint8_t multiplexer = channel_to_mux(channel);

    // Prepare the 16-bit configuration register value
    unsigned char config[3];
    config[0] = REG_CONFIG;
    config[1] = (multiplexer << 4) | (gain << 1) | 0x81; // Set OS bit to start a single conversion
    config[2] = (RATE_128 << 5) | 3; // Set rate to 128SPS and disable comparator

    if (write(i2c_handle, config, 3) != 3) {
        perror("ADS1115: Config write error");
        return -2;
    }

    // The ADS1115 datasheet says conversion time for 128SPS is ~7.8ms.
    // A 10ms sleep is more than enough time for the conversion to complete.
    usleep(10000);

    // Point to the conversion register to read the result
    config[0] = REG_CONV;
    if (write(i2c_handle, &config[0], 1) != 1) {
        perror("ADS1115: Address pointer write error");
        return -3;
    }

    // Read the 2-byte (16-bit) result
    unsigned char read_buf[2];
    if (read(i2c_handle, read_buf, 2) != 2) {
        perror("ADS1115: Conversion read error");
        return -4;
    }

    // Combine the two bytes into a single signed 16-bit integer
    *conversionResult = (int16_t)((read_buf[0] << 8) | read_buf[1]);
    return 0;
}

void ads1115_close(int i2c_handle) {
    if (i2c_handle >= 0) {
        close(i2c_handle);
    }
}
