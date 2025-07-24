#ifndef CALIBRATION_HELPER_H
#define CALIBRATION_HELPER_H

#include "stdio.h"
#include "pthread.h"
#include "signal.h"

// The number of ADC channels available for measurement and calibration.
#define NUM_CHANNELS 4

// Arguments for the calibration listener thread
typedef struct {
    int* sensor_index_ptr;
    pthread_mutex_t* mutex;
    volatile sig_atomic_t* keep_running_ptr;
} CalibrationThreadArgs;


int calibrateSensor(int index, int adc_reading, double *slope, double *offset);

// Listens for calibration commands from the user (e.g., "CAL0")
void *calibrationListener(void *args);

#endif
