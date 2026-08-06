cmake_minimum_required(VERSION 3.16.0)
set(PROJECT_VER "0.1.0")
set(SDKCONFIG_DEFAULTS
    "${CMAKE_CURRENT_LIST_DIR}/experiments/face_idf/sdkconfig.defaults")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hearing-chan)
