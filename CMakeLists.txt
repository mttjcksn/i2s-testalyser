cmake_minimum_required (VERSION 3.11)
project(i2s_testalyser)

add_definitions( -DLOGIC2 )

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.14" CACHE STRING "Minimum supported MacOS version" FORCE)

# enable generation of compile_commands.json, helpful for IDEs to locate include files.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# custom CMake Modules are located in the cmake directory.
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(ExternalAnalyzerSDK)

set(SOURCES 
src/I2sTestalyser.cpp
src/I2sTestalyser.h
src/I2sTestalyserResults.cpp
src/I2sTestalyserResults.h
src/I2sTestalyserSettings.cpp
src/I2sTestalyserSettings.h
src/I2sSimulationDataGenerator.cpp
src/I2sSimulationDataGenerator.h
)

add_analyzer_plugin(i2s_testalyser SOURCES ${SOURCES})
