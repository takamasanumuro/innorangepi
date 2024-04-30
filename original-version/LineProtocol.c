//Module to take a data set and convert it to a line protocol message for InfluxDB

#include "LineProtocol.h"

void setBucket(char* buffer, char* bucket) {
    sprintf(buffer, "%s", bucket);
}

void addTag(char* buffer, char* tagKey, char* tagValue) {
    sprintf(buffer, "%s,%s=%s", buffer, tagKey, tagValue);
}

void addField(char* buffer, char* fieldKey, float fieldValue) {
    bool hasWhiteSpace = false;

    for (int i = 0; i < strlen(buffer); i++) {
        if (isspace(buffer[i])) {
            hasWhiteSpace = true;
            break;
        }
    }

    if (hasWhiteSpace) {
        sprintf(buffer, "%s,%s=%f", buffer, fieldKey, fieldValue);
    } else {
        sprintf(buffer, "%s %s=%f", buffer, fieldKey, fieldValue);
    }
}

long getEpochSeconds() {
    time_t epoch_seconds;
    time(&epoch_seconds);
    return epoch_seconds;
}

//Add timestamp using epoch in seconds
void addTimestamp(char* buffer, long timestamp) {
    sprintf(buffer, "%s %ld", buffer, timestamp);
}

//Macro for unit test enable disable

int line_protocol_test() {
    char buffer[256];
    char bucket[10] = "myBucket";
    char tagKey[10] = "sensor";
    char tagValue[10] = "A0";
    char fieldKey[10] = "value";
    float fieldValue = 10.0;

    setBucket(buffer, bucket);
    addTag(buffer, tagKey, tagValue);
    addTag(buffer, "source", "instrumentacao");
    addField(buffer, fieldKey, fieldValue);
    addField(buffer, "tensao", 24.0f);
    addField(buffer, "corrente", 0.5f);
    addTimestamp(buffer, getEpochSeconds());
    
    printf("Line protocol message: %s\n", buffer);
    return 0;
}
