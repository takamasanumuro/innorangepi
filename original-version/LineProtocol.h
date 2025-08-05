#ifndef LINEPROTOCOL_H
#define LINEPROTOCOL_H

#include "stdio.h"
#include "string.h"
#include "stdbool.h"
#include "ctype.h"
#include "time.h"
#include "Measurement.h"

#define INFLUXDB_TOKEN_SIZE 256
#define INFLUXDB_BUCKET_SIZE 64
#define INFLUXDB_ORG_SIZE 64

typedef struct _InfluxDBContext {
    char bucket[INFLUXDB_BUCKET_SIZE];
    char org[INFLUXDB_ORG_SIZE];
    char token[INFLUXDB_TOKEN_SIZE];
} InfluxDBContext;

// --- NEW: Struct to hold GPS data ---
typedef struct {
    double latitude;
    double longitude;
    double altitude;
    double speed;
} GPSData;


int setMeasurement(char* buffer, size_t size, const char* measurement);
int addTag(char* buffer, size_t size, const char* tagKey, const char* tagValue);
int addField(char* buffer, size_t size, const char* fieldKey, double fieldValue);
long getEpochSeconds();
int addTimestamp(char* buffer, size_t size, long timestamp);


// --- MODIFIED: Added GPSData to the function signature ---
void sendDataToInfluxDB(const InfluxDBContext* dbContext, const Measurement* measurements, const MeasurementSetting* settings, const GPSData* gpsData);

// --- NEW: Function to send a compressed batch of data ---
bool sendCompressedBatchToInfluxDB(const InfluxDBContext* dbContext, const void* data, size_t size);

#endif
