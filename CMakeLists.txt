cmake_minimum_required(VERSION 3.0.0)
project(slo_measure VERSION 0.1.0)

include(CTest)
enable_testing()

add_executable(slo_measure main.cpp Connection.cpp Protocol.cpp util.cpp cmdline.cpp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

TARGET_LINK_LIBRARIES(slo_measure event)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
