cmake_minimum_required(VERSION 3.0.0)
project(resourcemanager VERSION 0.1.0)

add_library(resource_manager resource_manager.c control_scheme.c process_monitor.c)
target_link_libraries(resource_manager utils pqos pthread)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

add_subdirectory(log)
add_subdirectory(utils)
add_subdirectory(test)

add_executable(tester main.c)
target_link_libraries(tester resource_manager)