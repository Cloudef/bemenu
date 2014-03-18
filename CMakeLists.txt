CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(bemenu)
SET(BEMENU_NAME "bemenu")
SET(BEMENU_DESCRIPTION "Dynamic menu library and client program inspired by dmenu")
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${chck_SOURCE_DIR}/CMake/modules)

# Compile library
ADD_SUBDIRECTORY(lib)

# Compile client
ADD_SUBDIRECTORY(client)

# Compile tests
ADD_SUBDIRECTORY(test)

# Generate documentation (make doxygen)
ADD_SUBDIRECTORY(doxygen)

# vim: set ts=8 sw=4 tw=0
