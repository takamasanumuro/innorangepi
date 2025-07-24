#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Measurement.h"

// The number of ADC channels to configure.
#define NUM_CHANNELS 4

// Take measurements with multimeters and compare ADC vs Real Current to get a regression slope and offset for each sensor

void loadConfigurationFile(const char *filename, MeasurementSetting *settings) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening sensor configuration file");
        exit(EXIT_FAILURE);
    }

    char line[256];
    int line_num = 0;
    int settings_count = 0;

    // Read the file line by line until EOF or until we have loaded settings for all channels.
    while (fgets(line, sizeof(line), file) && settings_count < NUM_CHANNELS) {
        line_num++;
        // Ignore lines that are comments, empty, or headers.
        if (line[0] == '#' || line[0] == '\n' || line[0] == 'P' || line[0] == 'G') {
            continue;
        }

        char pin_name[16]; // To consume the "A0", "A1", etc., part of the line.

        // Use sscanf to parse the line, which is safer than a single fscanf for the whole file.
        // This provides better error isolation for malformed lines.
        int items_scanned = sscanf(line, "%s %lf %lf %15s %31s %15s",
                                   pin_name,
                                   &settings[settings_count].slope,
                                   &settings[settings_count].offset,
                                   settings[settings_count].gain_setting,
                                   settings[settings_count].id,
                                   settings[settings_count].unit);

        if (items_scanned == 6) {
            // If all 6 items were parsed successfully, move to the next setting.
            settings_count++;
        } else {
            // Warn the user if a line in the config file is malformed.
            fprintf(stderr, "Warning: Could not parse line %d in config file '%s'. Scanned %d items.\n", line_num, filename, items_scanned);
        }
    }

    if (settings_count < NUM_CHANNELS) {
        fprintf(stderr, "Warning: Config file '%s' contains settings for only %d out of %d channels.\n", filename, settings_count, NUM_CHANNELS);
    }

    fclose(file);
}
