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
	// set configuration for device setup
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

void TestDataRates(int i2c_handle)
{

	int rates[] = {8, 16, 32, 64, 128, 250, 475, 860};

	for (int rateIndex = 0; rateIndex < 8; rateIndex++)
	{
		// let`s calulate number of scan for 2 sec
		int numberScans =  rates[rateIndex] * 2;
		int averageValue = 0;

		printf("%d - Scan %4d samples at rate %3d Samples/sec for 2 sec  ...", rateIndex, numberScans, rates[rateIndex]);
		fflush(stdout);

		time_t now = time(NULL);
		int16_t conversionResult = 0;
		int16_t conversionSuccess = 1;
        int scanCounter = 0;

		for (; scanCounter < numberScans; scanCounter++)
		{
			conversionSuccess = readAdc(i2c_handle, AIN0, rateIndex, GAIN_1024MV, &conversionResult);

			if (conversionSuccess < 0)
			{
				printf("\tError %d during conversion.\n", conversionSuccess);
				break;
			}
			if (conversionResult > 32767) conversionResult -= 65535;
			averageValue += conversionResult;

		}

		int elapsedTime = time(NULL) - now;

		printf("---> got  %d Samples/sec with avg value of %.1f\n", scanCounter / elapsedTime, (float)averageValue / scanCounter); // *0.0021929906776);
	}
}

// Use this function to convert the ADC value to voltage or to calibrate the sensor based on measurements.
float linearCorrection(float value, float angular_coeff, float linear_coeff)
{
	return value * angular_coeff + linear_coeff;
}

// Function that takes a float value and returns a key-value string as "name=value"
char *floatToKeyValue(char *key, float value)
{
	char *str = malloc(100);
	sprintf(str, "%s=%.3f", key, value);
	return str;
}

// Function that takes a key-value and curls it into the server, in the route http://server_ip:port/ScadaBR/httpds?name=value
char *keyValueToURL(char *keyValue, char *server_ip, int server_port)
{
	char *url = malloc(100);
	sprintf(url, "http://%s:%d/ScadaBR/httpds?%s", server_ip, server_port, keyValue);
	return url;
}

//Function to curl the URL
void curlURL(char *url)
{
	CURL *curl_handle;
	CURLcode res;

	struct MemoryStruct chunk;
	chunk.memory = malloc(1);
	chunk.size = 0;

	curl_handle = curl_easy_init();
	if (curl_handle)
	{
		curl_easy_setopt(curl_handle, CURLOPT_URL, url);
		curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
		curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

		res = curl_easy_perform(curl_handle);

		if (res != CURLE_OK)
		{
			fprintf(stderr, "error: %s\n", curl_easy_strerror(res));
		}
		else
		{
			//printf("Size: %lu\n", (unsigned long)chunk.size);
			//printf("Data: %s\n", chunk.memory);
		}
		curl_easy_cleanup(curl_handle);
		free(chunk.memory);
	}
}

void sendMeasurementToServer(float value, char* tag, const char *targetURL, int targetPort) {
	char *measurementKeyValue = floatToKeyValue(tag, value);
	char *measurementURL = keyValueToURL(measurementKeyValue, targetURL, targetPort);
	curlURL(measurementURL);
}

int main (int argc, char **argv) {

	const char* program_name_str = argv[0];
	const char* i2c_address_str = argv[1];
	const char* config_file_str = argv[2];
	const char* device_bus_str = argv[3];
	const char* usage_str = "Usage: %s <I2C address> <config file>\n";
	
	/* 
	Which I2C bus is being used on Orange Pi or Raspberry Pi.
	Make sure to enable it on the device tree overlay, at /boot/orangePiEnv.txt or somewhere similar.
	*/
    char *device = "/dev/i2c-3"; 

	printf("%s", "\n********************************************\n");
	printf("Starting %s\n", program_name_str);
	if (argv[3] != NULL) {
		device = argv[3];
		printf("Device bus: %s\n", device_bus_str);
	} else {
		printf("No device bus provided. Using default: %s\n", device);
	}

    int i2c_address;
	if (argc < 2) {
		printf("No I2C address provided. Exiting...\n");
		return -1;
	}
	else {
		const char* i2c_address_str = argv[1]; //0x48 or 0x49
		if (strcmp(i2c_address_str, "0x48") == 0)
			i2c_address = 0x48;
		else if (strcmp(i2c_address_str, "0x49") == 0)
			i2c_address = 0x49;
		else {
			printf("Invalid I2C address. Exiting...\n");
			return -1;
		}
		printf("I2C address: %s\n\n\n", i2c_address_str);
	}

    int i2c_handle = open(device, O_RDWR);
    if (i2c_handle < 0)
    {
        printf("Unable to get I2C handle\n");
        return -1;
    }

	//System call to set the I2C slave address for communication
	ioctl(i2c_handle, I2C_SLAVE, i2c_address);


	// Load the configuration file with the correction values for each sensor
	char *config_file = argv[2];
	if (config_file == NULL) {
		printf("No config file provided. Exiting...\n");
		return -1;
	}

	MeasurementSetting settings[4];
	loadConfigurationFile(config_file, settings);

	printf("Configuration settings for board [%s]\n", config_file);
	for (int i = 0; i < 4; i++)
	{
		printf("[Correction A%d] Slope: %.6f, Offset: %.6f\t%s\t%s\n", i, settings[i].slope, settings[i].offset, settings[i].gain_setting, settings[i].id);
	}

	Measurement measurements[4];
	for (int i = 0; i < 4; i++) {
		setDefaultMeasurement(&measurements[i]);
		setMeasurementId(&measurements[i], settings[i].id);
		setMeasurementCorrection(&measurements[i], settings[i].slope, settings[i].offset);
	}


	// Listen to the user input to calibrate the sensors such as "CAL0" to calibrate the sensor at A0
	extern void *calibrationListener(void *args);
	int calibration_index = -1;
	pthread_t calibration_listener_thread;
	pthread_create(&calibration_listener_thread, NULL, calibrationListener, &calibration_index);

	while (1) {	

		// ADS1115 is being used as ADC with 15 bits of resolution on single ended mode, ie max value is 32767
		readAdc(i2c_handle, AIN0, RATE_128, gain_to_int(settings[0].gain_setting), &measurements[0].adc_value);
		readAdc(i2c_handle, AIN1, RATE_128, gain_to_int(settings[1].gain_setting), &measurements[1].adc_value);
		readAdc(i2c_handle, AIN2, RATE_128, gain_to_int(settings[2].gain_setting), &measurements[2].adc_value);
		readAdc(i2c_handle, AIN3, RATE_128, gain_to_int(settings[3].gain_setting), &measurements[3].adc_value);

		//Limit values above 15 bits to zero to avoid overflow conditions
		for (int i = 0; i < 4; i++) {
			if (measurements[i].adc_value > 32767) measurements[i].adc_value = 0;
		}


		// If the user has set a calibration index, calibrate the sensor at that index
		if (calibration_index >= 0) {

			//Suspends the calibration listener thread
			pthread_cancel(calibration_listener_thread);

			int calibration_success = calibrateSensor(calibration_index, measurements[calibration_index].adc_value);
			if (calibration_success) {
				printf("Calibration successful\n");
				setMeasurementCorrection(&measurements[calibration_index], 1, 0);
				calibration_index = -1;
				pthread_create(&calibration_listener_thread, NULL, calibrationListener, &calibration_index);
			} else {
				continue;
			}
		}


		printf("%s", "\n");
		for (int i = 0; i < 4; i++) {
			float calibrated_value = getMeasurementValue(&measurements[i]);
			printf("ADC%d\t%d\t%.2f%s\t%s\n", i, measurements[i].adc_value, calibrated_value, settings[i].unit, settings[i].id);
		}

		//char *server_ip = "44.221.0.169"; 
		//int server_port = 8080;

		//sendMeasurementToServer(battery_voltage_value, battery_voltage.id, server_ip, server_port);
		//sendMeasurementToServer(motor_port_current_value, motor_port_current.id, server_ip, server_port);
		//sendMeasurementToServer(motor_starboard_current_value, motor_starboard_current.id, server_ip, server_port);
		//sendMeasurementToServer(system_current_value, system_current.id, server_ip, server_port);
		sleep(1);
	}

	pthread_join(calibration_listener_thread, NULL); // Wait for the calibration listener to finish

	return 0 ;
}
