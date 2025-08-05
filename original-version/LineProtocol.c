#include "LineProtocol.h"
#include "util.h"
#include "OfflineQueue.h"
#include <string.h>
#include <stdlib.h>
#include <math.h> // For isnan
#include <curl/curl.h>

// This function is an internal implementation detail of this module,
// so it is declared as 'static' and not exposed in the header file.
// It now returns a boolean to indicate success or failure.
static bool CurlInfluxDB(const InfluxDBContext* dbContext, const char* lineProtocol) {
    CURL* curl_handle = curl_easy_init();
    if (!curl_handle) {
        fprintf(stderr, "Failed to initialize CURL\n");
        return false;
    }

    struct MemoryStruct chunk = { .memory = malloc(1), .size = 0 };
    if (!chunk.memory) {
        fprintf(stderr, "Failed to allocate memory for CURL response\n");
        curl_easy_cleanup(curl_handle);
        return false;
    }

    char url[256];
    snprintf(url, sizeof(url), "http://144.22.131.217:8086/api/v2/write?org=%s&bucket=%s&precision=s",
             dbContext->org, dbContext->bucket);

    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Token %s", dbContext->token);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: text/plain; charset=utf-8");

    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, lineProtocol);
    curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 2L);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 5L);

    CURLcode result = curl_easy_perform(curl_handle);
    bool success = (result == CURLE_OK);

    if (!success) {
        fprintf(stderr, "CURL error: %s\n", curl_easy_strerror(result));
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl_handle);
    free(chunk.memory);

    return success;
}

// --- NEW: Function to send a gzipped batch of line protocol data ---
bool sendCompressedBatchToInfluxDB(const InfluxDBContext* dbContext, const void* data, size_t size) {
    CURL* curl_handle = curl_easy_init();
    if (!curl_handle) {
        fprintf(stderr, "Failed to initialize CURL for batch send\n");
        return false;
    }

    struct MemoryStruct chunk = { .memory = malloc(1), .size = 0 };
    if (!chunk.memory) {
        fprintf(stderr, "Failed to allocate memory for CURL response\n");
        curl_easy_cleanup(curl_handle);
        return false;
    }

    char url[256];
    snprintf(url, sizeof(url), "http://144.22.131.217:8086/api/v2/write?org=%s&bucket=%s&precision=s",
             dbContext->org, dbContext->bucket);

    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Token %s", dbContext->token);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: text/plain; charset=utf-8");
    headers = curl_slist_append(headers, "Content-Encoding: gzip"); // Add GZIP header

    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    
    // Use CURLOPT_POSTFIELDSIZE since the compressed data is binary and not null-terminated
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, (long)size);

    // Use longer timeouts for potentially large batch uploads
    curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 30L);

    CURLcode result = curl_easy_perform(curl_handle);
    bool success = (result == CURLE_OK);
    if (!success) {
        fprintf(stderr, "CURL batch send error: %s\n", curl_easy_strerror(result));
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl_handle);
    free(chunk.memory);
    return success;
}


// --- MODIFIED: Function updated to include GPS data and use offline queue on failure ---
void sendDataToInfluxDB(const InfluxDBContext* dbContext, const Measurement* measurements, const MeasurementSetting* settings, const GPSData* gpsData) {
    char line_protocol_data[1024];
    setMeasurement(line_protocol_data, sizeof(line_protocol_data), "measurements");
    addTag(line_protocol_data, sizeof(line_protocol_data), "source", "instrumentacao");

    for (int i = 0; i < 4; i++) {
        if (strcmp(settings[i].id, "NC") != 0) {
            addField(line_protocol_data, sizeof(line_protocol_data), settings[i].id, getMeasurementValue(&measurements[i]));
        }
    }

    if (gpsData && !isnan(gpsData->latitude)) {
        addField(line_protocol_data, sizeof(line_protocol_data), "latitude", gpsData->latitude);
    }
    if (gpsData && !isnan(gpsData->longitude)) {
        addField(line_protocol_data, sizeof(line_protocol_data), "longitude", gpsData->longitude);
    }
    if (gpsData && !isnan(gpsData->altitude)) {
        addField(line_protocol_data, sizeof(line_protocol_data), "altitude", gpsData->altitude);
    }
    if (gpsData && !isnan(gpsData->speed)) {
        addField(line_protocol_data, sizeof(line_protocol_data), "speed", gpsData->speed);
    }

    addTimestamp(line_protocol_data, sizeof(line_protocol_data), getEpochSeconds());

    if (!CurlInfluxDB(dbContext, line_protocol_data)) {
        offline_queue_add(line_protocol_data);
    }
}


// --- Functions from original LineProtocol.c ---

int setMeasurement(char* buffer, size_t size, const char* measurement) {
    return snprintf(buffer, size, "%s", measurement);
}

int addTag(char* buffer, size_t size, const char* tagKey, const char* tagValue) {
    return snprintf(buffer + strlen(buffer), size - strlen(buffer), ",%s=%s", tagKey, tagValue);
}

int addField(char* buffer, size_t size, const char* fieldKey, double fieldValue) {
    char* fields_start = strchr(buffer, ' ');
    if (fields_start == NULL) {
        return snprintf(buffer + strlen(buffer), size - strlen(buffer), " %s=%.6f", fieldKey, fieldValue);
    } else {
        return snprintf(buffer + strlen(buffer), size - strlen(buffer), ",%s=%.6f", fieldKey, fieldValue);
    }
}

long getEpochSeconds() {
    return time(NULL);
}

int addTimestamp(char* buffer, size_t size, long timestamp) {
    return snprintf(buffer + strlen(buffer), size - strlen(buffer), " %ld", timestamp);
}
