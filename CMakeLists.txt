cmake_minimum_required(VERSION 3.16)

get_filename_component(idf_packages "$ENV{IDF_PATH}" DIRECTORY)
get_filename_component(platformio_core "${idf_packages}" DIRECTORY)
set(ENV{GIT_CEILING_DIRECTORIES} "${platformio_core};${CMAKE_SOURCE_DIR}")

include(cmake/cyberboat_quality.cmake)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(CyberBoat VERSION 1.0.0)