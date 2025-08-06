#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#define MEASUREMENT_ID_SIZE 32
#define GAIN_SETTING_SIZE 16
#define UNIT_SIZE 16
#define NUM_CHANNELS 4

typedef struct measurement {
	int adc_value;
	double _angular_correction;
	double _linear_correction;
	char id[MEASUREMENT_ID_SIZE];
} Measurement;

typedef struct {
    double slope;
    double offset;
	char gain_setting[GAIN_SETTING_SIZE];
	char id[MEASUREMENT_ID_SIZE];
	char unit[UNIT_SIZE];
} MeasurementSetting;

extern void setMeasurementId(Measurement *measurement, const char *id);
extern void setDefaultMeasurement(Measurement *measurement);
extern void setMeasurementCorrection(Measurement *measurement, double angular_correction, double linear_correction);
extern double getMeasurementValue(const Measurement *measurement);
extern void printMeasurement(const Measurement *measurement);
#endif
