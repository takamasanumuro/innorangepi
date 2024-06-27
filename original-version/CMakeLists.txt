#Lets create the build configuration for this folder

cmake_minimum_required(VERSION 3.0)
project(instrumentation-app)

#Add the source files to the build
set(SOURCES
    Measurement.c
    util.c
    ConfigurationLoader.c
    CalibrationHelper.c
    LineProtocol.c
    main.c
)

#Add the executable to the build
add_executable(instrumentation-app CalibrationHelper.c ${SOURCES})

#Link the libraries
find_package(CURL REQUIRED)
target_link_libraries(instrumentation-app PRIVATE CURL::libcurl)

#Copy the board configuration files to the build directory
file(GLOB CONFIG_FILES "${CMAKE_CURRENT_SOURCE_DIR}/config*.txt")
foreach (CONFIG_FILE ${CONFIG_FILES})
    get_filename_component(FILE_NAME ${CONFIG_FILE} NAME)
    file(COPY ${CONFIG_FILE} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/build)
endforeach()