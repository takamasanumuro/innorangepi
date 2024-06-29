#ifndef MEASUREMENT_H
#define MEASUREMENT_H
typedef struct measurement {
	int adc_value;
	double _converted_value;
	double _angular_correction;
	double _linear_correction;
	char *id;
} Measurement;

typedef struct {
    double slope;
    double offset;
	char gain_setting[16];
	char id[32];
	char unit[16];
} MeasurementSetting;

extern void setMeasurementId(Measurement *measurement, char *id);
extern void setDefaultMeasurement(Measurement *measurement);
extern void setMeasurementCorrection(Measurement *measurement, double angular_correction, double linear_correction);
extern void _convertMeasurement(Measurement *measurement);
extern double getMeasurementValue(Measurement *measurement);
extern void printMeasurement(Measurement *measurement);
#endif