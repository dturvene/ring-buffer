# cmake build file for the linux C ringbuffer project
# See rb.Dockerfile and rb-functions.sh

cmake_minimum_required(VERSION 3.10)
project(rb)

# for debugging
#SET(CMAKE_VERBOSE_MAKEFILE ON)

SET(SOURCES ringbuffer.c logevt.c)
SET(TARGET ringbuffer)

# https://cmake.org/cmake/help/latest/prop_sf/COMPILE_OPTIONS.html
#SET_SOURCE_FILES_PROPERTIES(${SOURCES} PROPERTIES
#  COMPILE_OPTIONS "-O2;-g;-Wall"
#  COMPILE_DEFINITIONS "DEBUG_LOGGING"
#  )

# https://cmake.org/cmake/help/latest/command/add_compile_options.html
ADD_COMPILE_OPTIONS(-O2 -g -Wall)
ADD_COMPILE_OPTIONS(-DDEBUG_LOGGING)

ADD_EXECUTABLE(${TARGET} ${SOURCES})
TARGET_LINK_LIBRARIES(${TARGET} pthread)

# https://cmake.org/cmake/help/v3.3/command/add_test.html
# NAME must be a single token 
ENABLE_TESTING()
ADD_TEST(NAME Validation COMMAND ${TARGET} -t 1)
ADD_TEST(NAME Simple_Stress COMMAND ${TARGET} -t 2)
ADD_TEST(NAME Large_Stress_Spinlock COMMAND ${TARGET} -t 3 -c 10000000)
ADD_TEST(NAME Large_Stress_Mutex COMMAND ${TARGET} -t 3 -c 10000000 -m)

