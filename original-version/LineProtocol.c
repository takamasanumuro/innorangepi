//Module to take a data set and convert it to a line protocol message for InfluxDB

#include "LineProtocol.h"
#include <string.h>

// Sets the measurement name. This should be the first part of the line protocol.
int setMeasurement(char* buffer, size_t size, const char* measurement) {
    // Using snprintf instead of sprintf to prevent buffer overflows.
    return snprintf(buffer, size, "%s", measurement);
}

// Adds a tag to the line protocol message. Tags are key-value pairs that are indexed.
int addTag(char* buffer, size_t size, const char* tagKey, const char* tagValue) {
    // Appends the tag to the existing buffer content.
    return snprintf(buffer + strlen(buffer), size - strlen(buffer), ",%s=%s", tagKey, tagValue);
}

// Adds a field to the line protocol message. Fields are the actual data points.
int addField(char* buffer, size_t size, const char* fieldKey, double fieldValue) {
    char* fields_start = strchr(buffer, ' ');
    
    // Check if any fields have been added yet. The first field is preceded by a space,
    // subsequent fields are preceded by a comma.
    if (fields_start == NULL) {
        // First field
        return snprintf(buffer + strlen(buffer), size - strlen(buffer), " %s=%.6f", fieldKey, fieldValue);
    } else {
        // Subsequent fields
        return snprintf(buffer + strlen(buffer), size - strlen(buffer), ",%s=%.6f", fieldKey, fieldValue);
    }
}

// Gets the current time as Unix epoch in seconds.
long getEpochSeconds() {
    return time(NULL);
}

// Adds a timestamp (in seconds) to the line protocol message.
int addTimestamp(char* buffer, size_t size, long timestamp) {
    return snprintf(buffer + strlen(buffer), size - strlen(buffer), " %ld", timestamp);
}

// Example test function to demonstrate usage.
int line_protocol_test() {
    char buffer[256];
    const char* measurement = "environment";
    
    setMeasurement(buffer, sizeof(buffer), measurement);
    addTag(buffer, sizeof(buffer), "sensor", "A0");
    addTag(buffer, sizeof(buffer), "source", "instrumentacao");
    addField(buffer, sizeof(buffer), "value", 10.0);
    addField(buffer, sizeof(buffer), "tensao", 24.0f);
    addField(buffer, sizeof(buffer), "corrente", 0.5f);
    addTimestamp(buffer, sizeof(buffer), getEpochSeconds());
    
    printf("Line protocol message: %s\n", buffer);
    return 0;
}
