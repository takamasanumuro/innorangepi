#include <stdio.h>
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

#ifdef __aarch64__
#include </usr/include/aarch64-linux-gnu/curl/curl.h>
//Check for x86_64
#elif __x86_64__
#include </usr/include/x86_64-linux-gnu/curl/curl.h>
#endif
#include "util.h"


// RATE
#define RATE_8   0
#define RATE_16  1
#define RATE_32  2
#define RATE_64  3
#define RATE_128 4
#define RATE_250 5
#define RATE_475 6
#define RATE_860 7

// GAIN
#define GAIN_6144MV 0
#define GAIN_4096MV 1
#define GAIN_2048MV 2
#define GAIN_1024MV 3
#define GAIN_512MV 4
#define GAIN_256MV 5
#define GAIN_256MV2 6
#define GAIN_256MV3 7

// multiplexer
#define AIN0_AIN1 0
#define AIN0_AIN3 1
#define AIN1_AIN3 2
#define AIN2_AIN3 3
#define AIN0      4
#define AIN1      5
#define AIN2      6
#define AIN3      7


// register address
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
	printf("Not supposed to print this.\n");
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

typedef struct measurement {
	int16_t adc_value;
	float _converted_value;
	float _angular_correction;
	float _linear_correction;
	char *id;
} Measurement;

void setMeasurementId(Measurement *measurement, char *id) {
	measurement->id = id;
}

void setDefaultMeasurement(Measurement *measurement) {
	measurement->adc_value = 0;
	measurement->_converted_value = 0.0f;
	measurement->_angular_correction = 1.0f;
	measurement->_linear_correction = 0.0f;
}

void setMeasurementCorrection(Measurement *measurement, float angular_correction, float linear_correction) {
	measurement->_angular_correction = angular_correction;
	measurement->_linear_correction = linear_correction;
}

void _convertMeasurement(Measurement *measurement) {
	measurement->_converted_value = measurement->adc_value * measurement->_angular_correction + measurement->_linear_correction; 
}

float getMeasurementValue(Measurement *measurement) {
	_convertMeasurement(measurement);
	return measurement->_converted_value;
}

void printMeasurement(Measurement *measurement) {
	printf("Measurement: %s\t", measurement->id);
	printf("ADC Value: %d\t", measurement->adc_value);
	printf("Converted Value: %.3f\n", measurement->_converted_value);
	printf("\n");
}

void sendMeasurementToServer(float value, char* tag, const char *targetURL, int targetPort) {
	char *measurementKeyValue = floatToKeyValue(tag, value);
	char *measurementURL = keyValueToURL(measurementKeyValue, targetURL, targetPort);
	curlURL(measurementURL);
}

int main (int argc, char **argv)
{
	
	printf("Starting %s\n", argv[0]);
    const char *device = "/dev/i2c-3"; // Depends on which I2C bus of the Orange Pi is being used. 
    const int i2c_address = 0x48;
    int i2c_handle;

    i2c_handle = open(device, O_RDWR);
    if (i2c_handle < 0)
    {
        printf("Unable to get I2C handle\n");
        return -1;
    }

	//System call to set the I2C slave address for communication
	ioctl(i2c_handle, I2C_SLAVE, i2c_address);

	while (1) {	
		
		Measurement battery_voltage, motor_port_current, motor_starboard_current, system_current;
		setDefaultMeasurement(&battery_voltage); setMeasurementId(&battery_voltage, "tensao");
		setDefaultMeasurement(&motor_port_current); setMeasurementId(&motor_port_current, "corrente-bombordo");
		setDefaultMeasurement(&motor_starboard_current); setMeasurementId(&motor_starboard_current, "corrente-boreste");
		setDefaultMeasurement(&system_current); setMeasurementId(&system_current, "corrente-sistema");

		// Take measurements with multimeters and compare ADC vs Real Current to get a regression slope and offset for each sensor
		// As these hall effect sensors are very linear, it only takes 3 measurements to get a R^2 > 0.99
		setMeasurementCorrection(&battery_voltage, 0.000915f, 0.207378f);
		setMeasurementCorrection(&motor_port_current, 0.00447980127056524f, -37.2283145463431f);
		setMeasurementCorrection(&motor_starboard_current, 0.00141428571428571f, 0.0956142857142856f);
		setMeasurementCorrection(&system_current, 0.00605987634233648, 0.305511877643996);

		// ADS1115 is being used as ADC with 15 bits of resolution on single ended mode.
		readAdc(i2c_handle, AIN0, RATE_128, GAIN_1024MV, &battery_voltage.adc_value);
		readAdc(i2c_handle, AIN1, RATE_128, GAIN_1024MV, &motor_port_current.adc_value);
		readAdc(i2c_handle, AIN2, RATE_128, GAIN_1024MV, &motor_starboard_current.adc_value);
		readAdc(i2c_handle, AIN3, RATE_128, GAIN_1024MV, &system_current.adc_value);

		
		float battery_voltage_value = getMeasurementValue(&battery_voltage);
		float motor_port_current_value = getMeasurementValue(&motor_port_current);
		float motor_starboard_current_value = getMeasurementValue(&motor_starboard_current);
		float system_current_value = getMeasurementValue(&system_current);
		
		printf("\nBattery Voltage: %.3f\n", battery_voltage_value);
		printf("Motor Port Current: %.3f\n", motor_port_current_value);
		printf("Motor Starboard Current: %.3f\n", motor_starboard_current_value);
		printf("System Current: %.3f\n", system_current_value);

		char *server_ip = "44.221.0.169"; 
		int server_port = 8080;

		sendMeasurementToServer(battery_voltage_value, battery_voltage.id, server_ip, server_port);
		sendMeasurementToServer(motor_port_current_value, motor_port_current.id, server_ip, server_port);
		sendMeasurementToServer(motor_starboard_current_value, motor_starboard_current.id, server_ip, server_port);
		sendMeasurementToServer(system_current_value, system_current.id, server_ip, server_port);
		sleep(1);
	}

	return 0 ;
}
