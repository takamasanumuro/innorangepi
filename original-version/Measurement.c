#include "Measurement.h"
#include <stdio.h>
#include <string.h> // For strncpy

// The function now takes a const char* for the id.
void setMeasurementId(Measurement *measurement, const char *id) {
	// Use strncpy for safe copying into the fixed-size array.
	strncpy(measurement->id, id, sizeof(measurement->id) - 1);
	// Ensure null-termination.
	measurement->id[sizeof(measurement->id) - 1] = '\0';
}

void setDefaultMeasurement(Measurement *measurement) {
	measurement->adc_value = 0;
	measurement->_converted_value = 0.0;
	measurement->_angular_correction = 1.0;
	measurement->_linear_correction = 0.0;
	measurement->id[0] = '\0'; // Initialize id as an empty string
}

void setMeasurementCorrection(Measurement *measurement, double angular_correction, double linear_correction) {
	measurement->_angular_correction = angular_correction;
	measurement->_linear_correction = linear_correction;
}

void _convertMeasurement(Measurement *measurement) {
	measurement->_converted_value = measurement->adc_value * measurement->_angular_correction + measurement->_linear_correction; 
}

double getMeasurementValue(Measurement *measurement) {
	_convertMeasurement(measurement);
	return measurement->_converted_value;
}

void printMeasurement(Measurement *measurement) {
	printf("Measurement: %s\t", measurement->id);
	printf("ADC Value: %d\t", measurement->adc_value);
	printf("Converted Value: %.3lf\n", measurement->_converted_value);
	printf("\n");
}
