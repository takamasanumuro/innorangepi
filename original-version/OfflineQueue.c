#include "OfflineQueue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <curl/curl.h>

#define OFFLINE_LOG_FILE "logs/offline_log.txt"
#define TEMP_LOG_FILE "logs/offline_log.tmp"

void offline_queue_init(void) {
    mkdir("logs", 0755);
}

void offline_queue_add(const char* line_protocol) {
    FILE* file = fopen(OFFLINE_LOG_FILE, "a");
    if (file) {
        fprintf(file, "%s\n", line_protocol);
        fclose(file);
        // printf("Network offline. Data saved to local queue.\n");
    } else {
        perror("Failed to open offline log file");
    }
}

void offline_queue_process(const InfluxDBContext* dbContext) {
    FILE* infile = fopen(OFFLINE_LOG_FILE, "r");
    if (!infile) {
        return; // No offline data to process
    }

    FILE* tmpfile = fopen(TEMP_LOG_FILE, "w");
    if (!tmpfile) {
        perror("Could not open temp file for offline queue processing");
        fclose(infile);
        return;
    }

    char line[2048];
    bool all_sent = true;

    while (fgets(line, sizeof(line), infile)) {
        // Remove newline character
        line[strcspn(line, "\n")] = 0;

        CURL* curl_handle = curl_easy_init();
        if (curl_handle) {
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
            curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, line);
            curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 1L);
            curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 1L);


            CURLcode res = curl_easy_perform(curl_handle);
            if (res != CURLE_OK) {
                fprintf(tmpfile, "%s\n", line); // Write back to temp file if sending failed
                all_sent = false;
            }

            curl_slist_free_all(headers);
            curl_easy_cleanup(curl_handle);
        } else {
            fprintf(tmpfile, "%s\n", line); // Could not init curl, save for later
            all_sent = false;
        }
    }

    fclose(infile);
    fclose(tmpfile);

    if (all_sent) {
        remove(OFFLINE_LOG_FILE); // All data sent, remove original file
        printf("Offline queue processed successfully.\n");
    } else {
        remove(OFFLINE_LOG_FILE);
        rename(TEMP_LOG_FILE, OFFLINE_LOG_FILE); // Some data failed, replace original with remaining data
        printf("Some offline data failed to send, will retry later.\n");
    }
}