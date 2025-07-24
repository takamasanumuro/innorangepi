#include "LineProtocol.h"
#include "util.h"
#include <string.h>
#include <stdlib.h>

// Include the curl library for HTTP requests
#ifdef __aarch64__
#include </usr/include/aarch64-linux-gnu/curl/curl.h>
#elif __x86_64__
#include </usr/include/x86_64-linux-gnu/curl/curl.h>
#elif __arm__
#include </usr/include/arm-linux-gnueabihf/curl/curl.h>
#else
#include <curl/curl.h>
#endif

// This function is an internal implementation detail of this module,
// so it is declared as 'static' and not exposed in the header file.
static void CurlInfluxDB(const InfluxDBContext* dbContext, const char* lineProtocol) {
    CURL* curl_handle = curl_easy_init();
    if (!curl_handle) {
        fprintf(stderr, "Failed to initialize CURL\n");
        return;
    }

    struct MemoryStruct chunk = { .memory = malloc(1), .size = 0 };
    if (!chunk.memory) {
        fprintf(stderr, "Failed to allocate memory for CURL response\n");
        curl_easy_cleanup(curl_handle);
        return;
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

    // --- ADDED TIMEOUTS ---
    // Set a timeout for the connection phase. If curl can't connect in 2
    // seconds, it will fail. This prevents getting stuck if the server is down.
    curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 2L);

    // Set a total timeout for the entire operation. If the whole request
    // (connect, send, receive) takes more than 5 seconds, it will fail.
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 5L);

    CURLcode result = curl_easy_perform(curl_handle);
    if (result != CURLE_OK) {
        // This will now report a timeout error if one occurs.
        fprintf(stderr, "CURL error: %s\n", curl_easy_strerror(result));
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl_handle);
    free(chunk.memory);
}

// Public function to format and send data.
void sendDataToInfluxDB(const InfluxDBContext* dbContext, const Measurement* measurements, const MeasurementSetting* settings) {
    char line_protocol_data[512];
    setMeasurement(line_protocol_data, sizeof(line_protocol_data), "measurements");
    addTag(line_protocol_data, sizeof(line_protocol_data), "source", "instrumentacao");

    for (int i = 0; i < 4; i++) {
        if (strcmp(settings[i].id, "NC") != 0) {
            addField(line_protocol_data, sizeof(line_protocol_data), settings[i].id, getMeasurementValue(&measurements[i]));
        }
    }
    addTimestamp(line_protocol_data, sizeof(line_protocol_data), getEpochSeconds());

    CurlInfluxDB(dbContext, line_protocol_data);
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
