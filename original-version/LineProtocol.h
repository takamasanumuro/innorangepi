#include "stdio.h"
#include "string.h"
#include "stdbool.h"
#include "ctype.h"
#include "time.h"

typedef struct _InfluxDBContext {
    char bucket[32];
    char org[32];
    char token[128];
} InfluxDBContext;

void setBucket(char* buffer, char* bucket);
void addTag(char* buffer, char* tagKey, char* tagValue);
void addField(char* buffer, char* fieldKey, float fieldValue);
long getEpochSeconds();
void addTimestamp(char* buffer, long timestamp);