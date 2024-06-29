/*
Module to help to calibrate the sensors. It shall take ADC readings and ask the user for the corresponding current or voltage.
It will take 3 measurements to perform a linear regression and calculate the slope and the offset of the sensor.
*/
#include "stdio.h"


int calibrateSensor(int index, int adc_reading, double *slope, double *offset);

//Listen to commands such as "CAL0" to calibrate the sensor at A0
void *calibrationListener(void *args);
