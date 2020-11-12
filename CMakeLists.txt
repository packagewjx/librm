cmake_minimum_required(VERSION 3.0.0)
project(resourcemanager VERSION 0.1.0)

# main program
add_executable(resourcemanager main.c)
target_link_libraries(resourcemanager pqos utils)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

add_library(process_monitor process_monitor.c)
target_link_libraries(process_monitor pqos pthread utils)

add_subdirectory(log)
add_subdirectory(utils)
add_subdirectory(test)

