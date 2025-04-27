cmake_minimum_required(VERSION 3.20)
project(openMSX
        VERSION 20.0
        LANGUAGES CXX)
set(OPENMSX_RELEASE_YEAR 2025)
set(OPENMSX_RELEASE_FLAG FALSE)

include(cmake/helpers.cmake)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}/conan")
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/conan")

add_subdirectory(src)

#TODO
#add_subdirectory(Contrib)
