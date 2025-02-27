#include <stdio.h> // Common input/output functions, such as printf
#include <unistd.h> // For the sleep function
#include <string.h> // For string manipulation
#include <math.h> // For mathematical functions
#include <stdint.h> // For integer types
#include <time.h> // For time functions
#include <sys/types.h> // For system calls 
#include <sys/stat.h>  // For system calls
#include <sys/ioctl.h>  // For system calls
#include <fcntl.h>  // For system calls
#include <linux/i2c-dev.h> // For I2C communication
#include <pthread.h> // For multithreading
#include "LineProtocol.h" //For InfluxDB line protocol
#include "CalibrationHelper.h" // For calibration functions

// Include the curl library for HTTP requests
#ifdef __aarch64__
#include </usr/include/aarch64-linux-gnu/curl/curl.h>
//Check for x86_64
#elif __x86_64__
#include </usr/include/x86_64-linux-gnu/curl/curl.h>
#elif __arm__
#include </usr/include/arm-linux-gnueabihf/curl/curl.h>
#endif

#include "Measurement.h" // Custom library for the Measurement struct
#include "util.h" // Curl functions


// RATE in SPS (samples per second)
#define RATE_8   0
#define RATE_16  1
#define RATE_32  2
#define RATE_64  3
#define RATE_128 4
#define RATE_250 5
#define RATE_475 6
#define RATE_860 7

// GAIN in mV, max expected voltage as input
#define GAIN_6144MV 0
#define GAIN_4096MV 1
#define GAIN_2048MV 2
#define GAIN_1024MV 3
#define GAIN_512MV 4
#define GAIN_256MV 5
#define GAIN_256MV2 6
#define GAIN_256MV3 7

//Convert ADC gain to corresponding index
int gain_to_int(char* gain_str) {
	if (strcmp(gain_str, "GAIN_6144MV") == 0) return GAIN_6144MV;
	if (strcmp(gain_str, "GAIN_4096MV") == 0) return GAIN_4096MV;
	if (strcmp(gain_str, "GAIN_2048MV") == 0) return GAIN_2048MV;
	if (strcmp(gain_str, "GAIN_1024MV") == 0) return GAIN_1024MV;
	if (strcmp(gain_str, "GAIN_512MV") == 0) return GAIN_512MV;
	if (strcmp(gain_str, "GAIN_256MV") == 0) return GAIN_256MV;
	if (strcmp(gain_str, "GAIN_256MV2") == 0) return GAIN_256MV2;
	if (strcmp(gain_str, "GAIN_256MV3") == 0) return GAIN_256MV3;
	return -1;

}

// Multiplexer settings
#define AIN0_AIN1 0
#define AIN0_AIN3 1
#define AIN1_AIN3 2
#define AIN2_AIN3 3
#define AIN0      4
#define AIN1      5
#define AIN2      6
#define AIN3      7


// Register addresses
#define  REG_CONV 0
#define  REG_CONFIG 1
#define  REG_LO_THRESH 2
#define  REG_HI_THRESH 3

/*
 *	readAdc
 *	Description: configures ADC and starts conversion. 
 *	Return value indicates success (0) or failure (<0).
 *
 *	i2c_handle: i2c peripheral file descriptor
 *	multiplexer: channel configuration
 *	i2c_handle: conversion rate
 *	i2c_handle: gain (1024mV, 2048mV, etc.)
 *	*conversionResult: the conversion result will be stored in this address
 *
*/
int16_t readAdc(int i2c_handle, uint8_t multiplexer, uint8_t rate, uint8_t gain, int16_t *conversionResult)
{
	// set configuration for i2c_bus setup
	unsigned char config[3];
	config[0] = REG_CONFIG; 	// opcode
	config[1] = (multiplexer << 4) | gain << 1 | 0x81;
	config[2] = rate << 5 | 3;

	if (write(i2c_handle, config, 3) != 3)
		return -1; //  error writing

	// waiting for data ready escape after 2 seconds
	time_t start = time(NULL);

	if (write(i2c_handle, config, 1) != 1)
		return -2;

	unsigned char msgFromAds1115 = 0;
	*conversionResult = 0;
	unsigned char conversionResultLE[] = {0U, 0U};
	while (1)
	{
		// too long...
		if ((time(NULL) - start) > 3) break; 

		// Waiting acknowledgement...
		if (read(i2c_handle, &msgFromAds1115, 1) != 1)
			return -3; //  error reading

		if (msgFromAds1115 & 0x80)
		{
			// start conversion
			config[0] = REG_CONV;
			if (write(i2c_handle, config, 1) != 1)
				return -4; // Error writing

			// Get conversion result
			if (read(i2c_handle, conversionResultLE, 2) != 2)
				return -5; //  error reading
			*conversionResult = (int16_t) (conversionResultLE[0]<<8) | conversionResultLE[1];
			return 0;
		}
	}
	printf("Problem reading I2C. Check board address and connections!\n");
	return -6 ; // not supposed to be there
}

// Use this function to convert the ADC value to voltage or to calibrate the sensor based on measurements.
double linearCorrection(double value, double angular_coeff, double linear_coeff)
{
	return value * angular_coeff + linear_coeff;
}

void CurlInfluxDB(InfluxDBContext dbContext, char* lineProtocol, char* ip, int port) {
	
	CURL* curl_handle;
	CURLcode result;

	struct MemoryStruct chunk;
	chunk.memory = malloc(1);
	chunk.size = 0;

	curl_handle = curl_easy_init();
	if (curl_handle) {
	
		char url[256];
		sprintf(url, "http://%s:%d/api/v2/write?org=%s&bucket=%s&precision=s", ip, port, dbContext.org, dbContext.bucket);

		char auth_header[256];
		sprintf(auth_header, "Authorization: Token %s", dbContext.token);
		struct curl_slist *header = NULL;
		header = curl_slist_append(header, auth_header);
			

		curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    	curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, header);
		curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
		curl_easy_setopt(curl_handle, CURLOPT_POST, 1L);
		curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, lineProtocol);
		//curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);

		result = curl_easy_perform(curl_handle);
		if (result != CURLE_OK) {
			fprintf(stderr, "CURL error: %s\n", curl_easy_strerror(result));
		} else {
			//printf("%s", "Data sent to InfluxDB\n");
			//printf ("Data: %s\n", chunk.memory);
		}

		curl_easy_cleanup(curl_handle);
		free(chunk.memory);
	}
}

void sendMeasurementToInfluxDB(Measurement* measurements, MeasurementSetting* settings, const char* url, int port) {
	InfluxDBContext dbContext = {"Innoboat", "Innomaker", "gK8YfMaAl54lo2sgZLiM3Y5CQStHip-7mBe5vbhh1no86k72B4Hqo8Tj1qAL4Em-zGRUxGwBWLkQd1MR9foZ-g=="};
	char* line_protocol_data[256];
	setBucket(line_protocol_data, dbContext.bucket);
	addTag(line_protocol_data, "source", "instrumentacao");
	for (int i = 0; i < 4; i++) {
		if (strcmp(settings[i].id, "NC") == 0) continue;
		addField(line_protocol_data, settings[i].id, getMeasurementValue(&measurements[i]));
	}

	CurlInfluxDB(dbContext, line_protocol_data, url, port);
}

int checkDevicePresence(const char* i2c_bus_string, int i2c_address) {

	//Parse the last integer from /dev/i2c-<bus> to check if the device is present
	char i2c_bus[32];
	strncpy(i2c_bus, i2c_bus_string, sizeof(i2c_bus));

	char *token = strtok(i2c_bus, "/");
	while (token != NULL) {
		strcpy(i2c_bus, token);
		token = strtok(NULL, "/");
	}

	token = strtok(i2c_bus, "-");
	while (token != NULL) {
		strcpy(i2c_bus, token);
		token = strtok(NULL, "-");
	}

	int i2c_bus_int = atoi(i2c_bus);

	char i2c_detect_command[100];
	sprintf(i2c_detect_command, "i2cdetect -y %d | grep -q %02x[^:]", i2c_bus_int, i2c_address);

	int result = system(i2c_detect_command);
	return result;
}

void printConfigurations(const char* config_file_str, MeasurementSetting* settings) {
	printf("Configuration settings for board [%s]\n", config_file_str);
	for (int i = 0; i < 4; i++) {
		printf("[Correction A%d] Slope: %.6f, Offset: %.6f\t%s\t%s\n", i, settings[i].slope, settings[i].offset, settings[i].gain_setting, settings[i].id);
	}
}

void applyConfigurations(MeasurementSetting* settings, Measurement* measurements) {
	for (int i = 0; i < 4; i++) {
		setDefaultMeasurement(&measurements[i]);
		setMeasurementId(&measurements[i], settings[i].id);
		setMeasurementCorrection(&measurements[i], settings[i].slope, settings[i].offset);
	}
}

void printMeasurements(Measurement* measurements, MeasurementSetting* settings) {
	printf("%s", "\n");
	for (int i = 0; i < 4; i++) {
		double calibrated_value = getMeasurementValue(&measurements[i]);
		printf("ADC%d\t%d\t%.2f%s\t%s\n", i, measurements[i].adc_value, calibrated_value, settings[i].unit, settings[i].id);
	}
}

void getMeasurements(int i2c_handle, MeasurementSetting* settings, Measurement* measurements) {
	// ADS1115 is being used as ADC with 15 bits of resolution on single ended mode, ie max value is 32767
	readAdc(i2c_handle, AIN0, RATE_128, gain_to_int(settings[0].gain_setting), &measurements[0].adc_value);
	readAdc(i2c_handle, AIN1, RATE_128, gain_to_int(settings[1].gain_setting), &measurements[1].adc_value);
	readAdc(i2c_handle, AIN2, RATE_128, gain_to_int(settings[2].gain_setting), &measurements[2].adc_value);
	readAdc(i2c_handle, AIN3, RATE_128, gain_to_int(settings[3].gain_setting), &measurements[3].adc_value);

	//Limit values above 15 bits to zero to avoid overflow conditions
	for (int i = 0; i < 4; i++) {
		if (measurements[i].adc_value > 32767) measurements[i].adc_value = 0;
	}
}

int main (int argc, char **argv) {

	const char* program_name_str = argv[0];
	const char* i2c_bus_str = argv[1];
	const char* i2c_address_str = argv[2];
	const char* config_file_str = argv[3];
	const char* usage_str = "Usage: %s <I2C bus> <i2c-address-hex> <config-file>\n";
	
	/* 
	Which I2C bus is being used on Orange Pi or Raspberry Pi.
	Make sure to enable it on the i2c_bus tree overlay, at /boot/orangePiEnv.txt or somewhere similar.
	*/

	printf("%s", "\n********************************************\n");
	printf("Starting %s\n", program_name_str);

    if (i2c_address_str != NULL) {
		printf("Device address: %s\n", i2c_address_str);
	} else {
		printf("No I2C address provided! Exiting...\n");
		return -1;
	
	}

	if (i2c_bus_str != NULL) {
		printf("Device bus: %s\n", i2c_bus_str);
	} else {
		printf("No I2C bus provided! Exiting...\n");
		return -1;
	}

	int i2c_address = strtol(i2c_address_str, NULL, 16);
	if (i2c_address == 0 || i2c_address == LONG_MAX || i2c_address == LONG_MIN) {
		printf("Invalid I2C address. Exiting...\n");
		return -1;
	}
	printf("I2C address: 0x%X\n\n\n", i2c_address);
	

    int i2c_handle = open(i2c_bus_str, O_RDWR);
    if (i2c_handle < 0)
    {
        perror("Error opening i2c bus");
        return -1;
    }


	if (checkDevicePresence(i2c_bus_str, i2c_address)) {
		printf("No I2C device found at address 0x%X. Exiting...\n", i2c_address);
		return -1;
	}

	//System call to set the I2C slave address for communication
	if (ioctl(i2c_handle, I2C_SLAVE, i2c_address)) {
		printf("Error setting I2C slave address. Exiting...\n");
		return -1;
	}

	// Load the configuration file with the correction values for each sensor
	if (config_file_str == NULL) {
		printf("No config file provided. Exiting...\n");
		return -1;
	}

	Measurement measurements[4];
	MeasurementSetting settings[4];
	
	loadConfigurationFile(config_file_str, settings);
	printConfigurations(config_file_str, settings);
	applyConfigurations(settings, measurements);

	// Listen to the user input to calibrate the sensors such as "CAL0" to calibrate the sensor at A0
	int calibration_sensor_index = -1;
	pthread_t calibration_listener_thread;
	pthread_create(&calibration_listener_thread, NULL, calibrationListener, &calibration_sensor_index);

	while (1) {	

		getMeasurements(i2c_handle, settings, measurements);
		printMeasurements(measurements, settings);

		// If the user has set a calibration index, calibrate the sensor at that index
		if (calibration_sensor_index >= 0) {

			//Suspends the calibration listener thread
			pthread_cancel(calibration_listener_thread);

			double slope = 0.0, offset = 0.0;
			int calibration_success = calibrateSensor(calibration_sensor_index, measurements[calibration_sensor_index].adc_value, &slope, &offset);
			if (calibration_success) {
				printf("Calibration successful\n");
				setMeasurementCorrection(&measurements[calibration_sensor_index], slope, offset);
				calibration_sensor_index = -1;
				pthread_create(&calibration_listener_thread, NULL, calibrationListener, &calibration_sensor_index);
			} else {
				continue;
			}
		}

		//char *server_ip = "144.22.131.217"; 
		//int server_port = 8086;
		//sendMeasurementToInfluxDB(measurements, settings, server_ip, server_port);

		sleep(1);
	}

	pthread_join(calibration_listener_thread, NULL); // Wait for the calibration listener to finish

	return 0 ;
}
