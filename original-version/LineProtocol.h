#ifndef LINEPROTOCOL_H
#define LINEPROTOCOL_H

#include "stdio.h"
#include "string.h"
#include "stdbool.h"
#include "ctype.h"
#include "time.h"

// Increased size for token to be safe
#define INFLUXDB_TOKEN_SIZE 256
#define INFLUXDB_BUCKET_SIZE 64
#define INFLUXDB_ORG_SIZE 64

typedef struct _InfluxDBContext {
    char bucket[INFLUXDB_BUCKET_SIZE];
    char org[INFLUXDB_ORG_SIZE];
    char token[INFLUXDB_TOKEN_SIZE];
} InfluxDBContext;

int setMeasurement(char* buffer, size_t size, const char* measurement);
int addTag(char* buffer, size_t size, const char* tagKey, const char* tagValue);
int addField(char* buffer, size_t size, const char* fieldKey, double fieldValue);
long getEpochSeconds();
int addTimestamp(char* buffer, size_t size, long timestamp);

#endif
