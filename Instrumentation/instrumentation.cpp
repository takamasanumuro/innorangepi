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

int TestDataRates(int i2c_handle)
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

		for (scanCounter; scanCounter < numberScans; scanCounter++)
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

