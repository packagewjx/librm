cmake_minimum_required(VERSION 3.0.0)
project(resourcemanager VERSION 0.1.0)

# pqos_utils
add_library(pqos_utils pqos_utils.c)
target_link_libraries(pqos_utils pqos)

# main program
add_executable(resourcemanager main.c)
target_link_libraries(resourcemanager pqos pqos_utils)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

add_subdirectory(test)

