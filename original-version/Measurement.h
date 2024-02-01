#ifndef MEASUREMENT_H
#define MEASUREMENT_H
typedef struct measurement {
	int adc_value;
	float _converted_value;
	float _angular_correction;
	float _linear_correction;
	char *id;
} Measurement;

typedef struct {
    float slope;
    float offset;
} MeasurementCorrection;

extern void setMeasurementId(Measurement *measurement, char *id);
extern void setDefaultMeasurement(Measurement *measurement);
extern void setMeasurementCorrection(Measurement *measurement, float angular_correction, float linear_correction);
extern void _convertMeasurement(Measurement *measurement);
extern float getMeasurementValue(Measurement *measurement);
extern void printMeasurement(Measurement *measurement);
#endif