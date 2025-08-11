#include "ApplicationManager.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

#define I2C_MAX_ADDRESS 0x7F

// Global pointer to the app manager, allowing the signal handler to access it.
static ApplicationManager* g_app_manager = NULL;

/**
 * @brief Signal handler to catch SIGINT and SIGTERM for graceful shutdown.
 */
static void signal_handler(int signum) {
    if (!g_app_manager) return;
    app_manager_signal_shutdown(g_app_manager);
}

/**
 * @brief Prints a usage error message to stderr.
 */
static int usage_error(const char* prog_name) {
    fprintf(stderr, "Usage: %s <i2c-bus> <i2c-address-hex> <config-file>\n", prog_name);
    return 1;
}

/**
 * @brief The main entry point of the application.
 */
int main(int argc, char **argv) {
    // Checks if correct number of arguments was provided
    if (argc != 4) {
        return usage_error(argv[0]);
    }

    // I2C address range validation
    errno = 0; //Will be set if strtol fails due to invalid input
    char* endptr;
    long i2c_address = strtol(argv[2], &endptr, 16);
    if (errno != 0 || *endptr != '\0' || i2c_address < 0 || i2c_address > I2C_MAX_ADDRESS) {
        fprintf(stderr, "Invalid I2C address: %s\n", argv[2]);
        return 1;
    }

    // File accessibility validation before initialization
    if (access(argv[3], R_OK) != 0) {
        perror("Config file not accessible");
        return 1;
    }

    // Set up signal handling for graceful shutdown
    // sigaction is used to due to increased portability and safety against race conditions
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask); // Don't block other signals in the handler
    sa.sa_flags = 0; // Or SA_RESTART to auto-restart syscalls

    if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("Failed to register signal handlers");
        return 1;
    }

    // Create and initialize the application manager.
    g_app_manager = app_manager_create(argv[1], i2c_address, argv[3]);
    if (!g_app_manager) {
        fprintf(stderr, "[Main] Application creation failed. Exiting.\n");
        return 1;
    }
    
    AppManagerError init_result = app_manager_init(g_app_manager);
    if (init_result != APP_SUCCESS) {
        fprintf(stderr, "[Main] Application initialization failed: %s\n", 
                app_manager_error_string(init_result));
        app_manager_destroy(g_app_manager);
        return 1;
    }

    // Run the main application loop.
    app_manager_run(g_app_manager);

    // Clean up and destroy the application manager.
    app_manager_destroy(g_app_manager);

    printf("[Main] Shutdown complete.\n");
    return 0;
}