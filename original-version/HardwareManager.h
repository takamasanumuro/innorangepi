#ifndef HARDWARE_MANAGER_H
#define HARDWARE_MANAGER_H

#include <gps.h>
#include <stdbool.h>

typedef struct {
    int i2c_handle;
    struct gps_data_t gps_data;
    bool gps_connected;
    char i2c_bus_path[256];  // Store for debugging/logging
    long i2c_address;
} HardwareManager;

// Initialize hardware subsystems
bool hardware_manager_init(HardwareManager* hw_manager, 
                          const char* i2c_bus_path, 
                          long i2c_address);

// Cleanup hardware resources
void hardware_manager_cleanup(HardwareManager* hw_manager);

// Accessors for hardware handles
int hardware_manager_get_i2c_handle(const HardwareManager* hw_manager);
struct gps_data_t* hardware_manager_get_gps_data(HardwareManager* hw_manager);
bool hardware_manager_is_gps_connected(const HardwareManager* hw_manager);

#endif // HARDWARE_MANAGER_H