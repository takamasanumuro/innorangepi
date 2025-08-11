#ifndef CSVLOGGER_H
#define CSVLOGGER_H

#include <stdio.h>
#include <stdbool.h>
#include "Measurement.h"
#include "DataPublisher.h"

// A structure to hold the state of the CSV logger
typedef struct {
    FILE* file_handle;
    bool is_active;
} CsvLogger;

/**
 * @brief Initializes the CSV logger.
 * * Checks for the 'CSV_LOGGING_ENABLE' environment variable. If it's set to '1' or 'true',
 * this function creates a new CSV file with a timestamped name in the 'logs' directory
 * and writes the header row.
 * * @param logger A pointer to the CsvLogger instance to initialize.
 * @param channels A pointer to the array of Channel to get the column names for the header.
 */
void csv_logger_init(CsvLogger* logger, const Channel* channels);

/**
 * @brief Logs a row of data to the CSV file.
 * * If the logger is active, this function writes the current timestamp, sensor measurements,
 * and GPS data as a new row in the CSV file.
 * * @param logger A pointer to the CsvLogger instance.
 * @param measurements A pointer to the array of current measurements.
 * @param gps_data A pointer to the current GPS data.
 */
void csv_logger_log(const CsvLogger* logger, const Channel* channels, const GPSData* gps_data);

/**
 * @brief Closes the CSV logger file.
 * * If the logger is active, this function will close the file handle.
 * * @param logger A pointer to the CsvLogger instance.
 */
void csv_logger_close(CsvLogger* logger);

#endif // CSVLOGGER_H
