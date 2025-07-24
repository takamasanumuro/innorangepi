#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#define MEASUREMENT_ID_SIZE 32
#define GAIN_SETTING_SIZE 16
#define UNIT_SIZE 16

typedef struct measurement {
	int adc_value;
	double _converted_value;
	double _angular_correction;
	double _linear_correction;
	// Changed to a fixed-size array for better memory management
	char id[MEASUREMENT_ID_SIZE];
} Measurement;

typedef struct {
    double slope;
    double offset;
	char gain_setting[GAIN_SETTING_SIZE];
	char id[MEASUREMENT_ID_SIZE];
	char unit[UNIT_SIZE];
} MeasurementSetting;

// Changed to accept a const char* to match how it's called.
// This fixes the -Wdiscarded-qualifiers warning.
extern void setMeasurementId(Measurement *measurement, const char *id);
extern void setDefaultMeasurement(Measurement *measurement);
extern void setMeasurementCorrection(Measurement *measurement, double angular_correction, double linear_correction);
extern void _convertMeasurement(Measurement *measurement);
extern double getMeasurementValue(Measurement *measurement);
extern void printMeasurement(Measurement *measurement);
#endif
