# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project("BlueRetro$ENV{BR_HW}$ENV{BR_SYS}")

if("$ENV{BR_WOKWI}" STREQUAL "1")
    idf_build_set_property(COMPILE_DEFINITIONS "-DBLUERETRO_WOKWI" APPEND)
endif()