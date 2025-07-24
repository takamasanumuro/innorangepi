#include "Measurement.h"
#include <stdio.h>
#include <string.h> // For strncpy

void setMeasurementId(Measurement *measurement, const char *id) {
	strncpy(measurement->id, id, sizeof(measurement->id) - 1);
	measurement->id[sizeof(measurement->id) - 1] = '\0';
}

void setDefaultMeasurement(Measurement *measurement) {
	measurement->adc_value = 0;
	measurement->_angular_correction = 1.0;
	measurement->_linear_correction = 0.0;
	measurement->id[0] = '\0';
}

void setMeasurementCorrection(Measurement *measurement, double angular_correction, double linear_correction) {
	measurement->_angular_correction = angular_correction;
	measurement->_linear_correction = linear_correction;
}


double getMeasurementValue(const Measurement *measurement) {
	return measurement->adc_value * measurement->_angular_correction + measurement->_linear_correction;
}

void printMeasurement(const Measurement *measurement) {
	printf("Measurement: %s\t", measurement->id);
	printf("ADC Value: %d\t", measurement->adc_value);
	printf("Converted Value: %.3lf\n", getMeasurementValue(measurement));
	printf("\n");
}
