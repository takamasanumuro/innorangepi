/*
Module to help to calibrate the sensors. It shall take ADC readings and ask the user for the corresponding current or voltage.
It will take 3 measurements to perform a linear regression and calculate the slope and the offset of the sensor.
*/

#include "stdio.h"

int calibrateSensor(int index, int adc_reading) {
    static int counter = 0;
    static float adc_readings[3];
    static float real_readings[3];
    float slope = 0;
    float offset = 0;
    
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
    scanf("%f", &real_readings[counter]);

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
    slope = (real_readings[2] - real_readings[0]) / (adc_readings[2] - adc_readings[0]);
    offset = real_readings[0] - slope * adc_readings[0];
    printf("The slope is %f and the offset is %f\n", slope, offset);

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



/*Unit Test

int main() {
    calibrateSensor("battery_voltage", 10000);
    calibrateSensor("battery_voltage", 20000);
    calibrateSensor("battery_voltage", 30000);
    return 0;
}

*/