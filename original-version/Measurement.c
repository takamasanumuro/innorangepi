#include "Measurement.h"
#include <stdio.h>

void setMeasurementId(Measurement *measurement, char *id) {
	measurement->id = id;
}

void setDefaultMeasurement(Measurement *measurement) {
	measurement->adc_value = 0;
	measurement->_converted_value = 0.0;
	measurement->_angular_correction = 1.0;
	measurement->_linear_correction = 0.0;
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