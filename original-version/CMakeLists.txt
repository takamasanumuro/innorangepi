#Lets create the build configuration for this folder

cmake_minimum_required(VERSION 3.0)
project(instrumentation-app)

#Add the source files to the build
set(SOURCES
    Measurement.c
    util.c
    ConfigurationLoader.c
    main.c
)

#Add the executable to the build
add_executable(instrumentation-app ${SOURCES})

#Link the libraries
find_package(CURL REQUIRED)
target_link_libraries(instrumentation-app PRIVATE CURL::libcurl)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/configA.txt DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/configB.txt DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/configC.txt DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/configD.txt DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/configE.txt DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
