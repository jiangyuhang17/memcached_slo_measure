cmake_minimum_required(VERSION 3.0.0)
project(slo_benchmark VERSION 0.1.0)

include(CTest)
enable_testing()

add_executable(slo_benchmark main.cpp Connection.cpp Protocol.cpp util.cpp cmdline.cpp)

TARGET_LINK_LIBRARIES(slo_benchmark event)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
