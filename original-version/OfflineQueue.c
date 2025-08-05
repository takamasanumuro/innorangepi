#include "OfflineQueue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h> // For gzip compression

#define OFFLINE_LOG_FILE "logs/offline_log.txt"
#define TEMP_LOG_FILE "logs/offline_log.tmp"
#define MAX_BATCH_SIZE 5000
#define MAX_LINE_LENGTH 2048
// Allocate a generous buffer for the uncompressed batch (5000 lines * 2KB/line)
#define BATCH_BUFFER_SIZE (MAX_BATCH_SIZE * MAX_LINE_LENGTH)

void offline_queue_init(void) {
    mkdir("logs", 0755);
}

void offline_queue_add(const char* line_protocol) {
    FILE* file = fopen(OFFLINE_LOG_FILE, "a");
    if (file) {
        fprintf(file, "%s\n", line_protocol);
        fclose(file);
        printf("Network offline. Data saved to local queue.\n");
    } else {
        perror("Failed to open offline log file");
    }
}

// Helper function to process a batch of lines
static bool process_batch(const InfluxDBContext* dbContext, char** lines, int line_count) {
    if (line_count <= 0) {
        return true;
    }

    // 1. Concatenate lines into a single buffer, separated by newlines
    char* uncompressed_buffer = malloc(BATCH_BUFFER_SIZE);
    if (!uncompressed_buffer) {
        perror("Failed to allocate memory for uncompressed batch");
        return false;
    }
    size_t current_size = 0;
    for (int i = 0; i < line_count; i++) {
        // Use the line as is, since it should already have a newline from fgets
        size_t line_len = strlen(lines[i]);
        if (current_size + line_len < BATCH_BUFFER_SIZE) {
            memcpy(uncompressed_buffer + current_size, lines[i], line_len);
            current_size += line_len;
        } else {
            fprintf(stderr, "Batch buffer overflow, truncating batch.\n");
            break;
        }
    }

    // 2. Compress the buffer using zlib
    uLong compressed_size = compressBound(current_size);
    Bytef* compressed_buffer = malloc(compressed_size);
    if (!compressed_buffer) {
        perror("Failed to allocate memory for compressed batch");
        free(uncompressed_buffer);
        return false;
    }

    int z_result = compress(compressed_buffer, &compressed_size, (const Bytef*)uncompressed_buffer, current_size);
    if (z_result != Z_OK) {
        fprintf(stderr, "zlib compression failed: %d\n", z_result);
        free(uncompressed_buffer);
        free(compressed_buffer);
        return false;
    }

    // 3. Send the compressed data
    printf("Sending compressed batch (%zu bytes -> %lu bytes)...\n", current_size, compressed_size);
    bool success = sendCompressedBatchToInfluxDB(dbContext, compressed_buffer, compressed_size);

    // 4. Cleanup
    free(uncompressed_buffer);
    free(compressed_buffer);

    return success;
}

void offline_queue_process(const InfluxDBContext* dbContext) {
    FILE* infile = fopen(OFFLINE_LOG_FILE, "r");
    if (!infile) {
        return; // No offline data to process
    }

    // Check if file is empty before proceeding to avoid unnecessary processing
    fseek(infile, 0, SEEK_END);
    if (ftell(infile) == 0) {
        fclose(infile);
        return;
    }
    fseek(infile, 0, SEEK_SET);

    printf("Processing offline data queue...\n");

    FILE* tmpfile = fopen(TEMP_LOG_FILE, "w");
    if (!tmpfile) {
        perror("Could not open temp file for offline queue processing");
        fclose(infile);
        return;
    }

    char line[MAX_LINE_LENGTH]; // mutable fixed maximum size char array to act as a buffer for line strings
    char* line_batch[MAX_BATCH_SIZE]; //array of pointers to hold the addresses of the duplicated strings
    int line_count = 0;
    bool any_batch_failed = false;

    while (fgets(line, sizeof(line), infile)) {
        line_batch[line_count] = strdup(line); // Duplicate the line storing it in an allocated memory and make the first pointer point to its address
        if (!line_batch[line_count]) {
            perror("strdup failed in offline queue");
            for(int i = 0; i < line_count; i++) free(line_batch[i]);
            any_batch_failed = true; // Mark as failed to preserve rest of file
            break;
        }
        line_count++;

        if (line_count == MAX_BATCH_SIZE) {
            if (process_batch(dbContext, line_batch, line_count)) {
                printf("Successfully sent a batch of %d offline points.\n", line_count);
            } else {
                printf("Failed to send a batch of %d offline points. Saving them for later.\n", line_count);
                any_batch_failed = true;
                for (int i = 0; i < line_count; i++) {
                    fputs(line_batch[i], tmpfile);
                }
            }
            for (int i = 0; i < line_count; i++) free(line_batch[i]);
            line_count = 0;
        }
    }

    // Process any remaining lines that didn't form a full batch
    if (line_count > 0) {
        if (process_batch(dbContext, line_batch, line_count)) {
            printf("Successfully sent the final batch of %d offline points.\n", line_count);
        } else {
            printf("Failed to send the final batch of %d offline points. Saving them for later.\n", line_count);
            any_batch_failed = true;
            for (int i = 0; i < line_count; i++) {
                fputs(line_batch[i], tmpfile);
            }
        }
        for (int i = 0; i < line_count; i++) free(line_batch[i]);
    }

    fclose(infile);
    fclose(tmpfile);

    remove(OFFLINE_LOG_FILE); // Remove the old log file

    if (any_batch_failed) {
        rename(TEMP_LOG_FILE, OFFLINE_LOG_FILE); // Rename temp file to be the new log file
    } else {
        remove(TEMP_LOG_FILE); // If all succeeded, no need for the temp file
        printf("Offline queue fully processed.\n");
    }
}
