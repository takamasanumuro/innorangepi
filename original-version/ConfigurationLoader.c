#include <stdio.h>
#include <stdlib.h>
#include "Measurement.h"


// Take measurements with multimeters and compare ADC vs Real Current to get a regression slope and offset for each sensor
// As these hall effect sensors are very linear, it only takes 3 measurements to get a R^2 > 0.99

void loadConfigurationFile(const char *filename, MeasurementCorrection *correction) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    while (fscanf(file, "%*s %f %f)", &correction->slope, &correction->offset) == 2) {
        printf("Slope: %f, Offset: %f\n", correction->slope, correction->offset);
        correction++;
    }

    fclose(file);
}

