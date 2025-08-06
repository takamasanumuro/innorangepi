#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <pthread.h>
#include "Measurement.h"
#include "LineProtocol.h" // For GPSData

// This structure will hold the latest data from all sources.
// It will be protected by a mutex to ensure thread-safe access.
typedef struct {
    pthread_mutex_t mutex;
    Measurement measurements[NUM_CHANNELS];
    GPSData gps_data;
} SharedData;

// Global instance of our shared data
extern SharedData g_shared_data;

#endif // SHARED_DATA_H
