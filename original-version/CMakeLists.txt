cmake_minimum_required(VERSION 3.0)
project(instrumentation-app)

#Add the source files to the build
set(SOURCES
    Measurement.c
    util.c
    ConfigurationLoader.c
    CalibrationHelper.c
    LineProtocol.c
    ADS1115.c
    CsvLogger.c
    OfflineQueue.c
    SocketServer.c
    main.c
)

#Add the executable to the build
add_executable(instrumentation-app ${SOURCES})

#Link the libraries
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
target_link_libraries(instrumentation-app PRIVATE CURL::libcurl Threads::Threads gps ZLIB::ZLIB)

#Copy the board configuration files to the build directory
# This ensures that when you run the app from the build directory, it can find the config files.
file(GLOB CONFIG_FILES "${CMAKE_CURRENT_SOURCE_DIR}/config*")
foreach (CONFIG_FILE ${CONFIG_FILES})
    get_filename_component(FILE_NAME ${CONFIG_FILE} NAME)
    file(COPY ${CONFIG_FILE} DESTINATION ${CMAKE_BINARY_DIR})
endforeach()