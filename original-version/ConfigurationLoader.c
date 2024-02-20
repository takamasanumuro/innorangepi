#include <stdio.h>
#include <stdlib.h>
#include "Measurement.h"

//Measurement correction has the following format:

//sensor_name slope offset
//battery_voltage 0.000915 0.207378
//motor_port_current 0.00447980127056524 -37.2283145463431
//motor_starboard_current 0.00141428571428571 0.0956142857142856
//system_current 0.00605987634233648 0.305511877643996

// Take measurements with multimeters and compare ADC vs Real Current to get a regression slope and offset for each sensor
// As these hall effect sensors are very linear, it only takes 3 measurements to get a R^2 > 0.99

void loadConfigurationFile(const char *filename, MeasurementCorrection *correction) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening sensor configuration file");
        exit(EXIT_FAILURE);
    }

    while (fscanf(file, "%*s %f %f)", &correction->slope, &correction->offset) == 2) {
        printf("Slope: %f, Offset: %f\n", correction->slope, correction->offset);
        correction++;
    }

    fclose(file);
}

