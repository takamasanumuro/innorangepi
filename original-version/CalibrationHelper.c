/*
Module to help to calibrate the sensors. It shall take ADC readings and ask the user for the corresponding current or voltage.
It will take 3 measurements to perform a linear regression and calculate the slope and the offset of the sensor.
*/

#include "stdio.h"

int calibrateSensor(int index, int adc_reading) {
    static int counter = 0;
    static double adc_readings[3];
    static double physical_readings[3];
    double slope = 0.0;
    double offset = 0.0;
    
    if (counter == 0) {
        printf("***********************\n");
        printf("Calibrating sensor at A%d\n", index);
        printf("3 measurements are needed to calibrate the sensor\n");
        printf("Change the current or voltage for each measurement\n");
        printf("***********************\n");
    }

    adc_readings[counter] = adc_reading;
    printf("Current ADC reading: %d\n", adc_reading);
    printf("Please enter the real reading[%d]: ", counter);
    scanf("%lf", &physical_readings[counter]);

    printf("Change the current or voltage for the next measurement and press 1\n");
    int next = 0;
    while (!next) {
        scanf("%d", &next);
    }
    

    if (counter < 2) {
        counter++;
        return 0;
    }

    // Calculate the slope and the offset
    slope = (physical_readings[2] - physical_readings[0]) / (adc_readings[2] - adc_readings[0]);
    offset = physical_readings[0] - slope * adc_readings[0];
    printf("The slope is %lf and the offset is %lf\n", slope, offset);

    // Reset the counter and return valid calibration
    counter = 0;
    return 1;
}

//Listen to commands such as "CAL0" to calibrate the sensor at A0

void *calibrationListener(void *args) {
    char command[5];
    int* sensorIndex = (int*) args;

    while (1) {
        scanf("%s", command);
        if (sscanf(command, "CAL%d", sensorIndex) == 1) {
            printf("Index set at A%d for calibration\n", *sensorIndex);
        }
    }
}

#include <stdio.h>

void least_squares(int n, double x[], double y[], double *m, double *b) {
    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0;
    
    for (int i = 0; i < n; i++) {
        sum_x += x[i];
        sum_y += y[i];
        sum_xy += x[i] * y[i];
        sum_x2 += x[i] * x[i];
    }
    
    *m = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
    *b = (sum_y - (*m) * sum_x) / n;
}

/*
int main() {
    int n;
    printf("Enter the number of data points: ");
    scanf("%d", &n);

    double x[n], y[n];
    
    printf("Enter the x values:\n");
    for (int i = 0; i < n; i++) {
        scanf("%lf", &x[i]);
    }
    
    printf("Enter the y values:\n");
    for (int i = 0; i < n; i++) {
        scanf("%lf", &y[i]);
    }
    
    double m, b;
    least_squares(n, x, y, &m, &b);
    
    printf("The best-fit line is: y = %lf * x + %lf\n", m, b);
    
    return 0;
}
*/