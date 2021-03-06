cmake_minimum_required(VERSION 3.0.0)
project(resourcemanager VERSION 0.1.0)

include_directories(lib/log/src)
include_directories(lib/hashmap)

add_library(resource_manager SHARED resource_manager.c control_scheme.c process_monitor.c $<TARGET_OBJECTS:utils> $<TARGET_OBJECTS:log> perfmem.c rth.c)
set_target_properties(resource_manager PROPERTIES PUBLIC_HEADER resource_manager.h)
target_link_libraries(resource_manager pqos pthread)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

add_subdirectory(lib/log)
add_subdirectory(utils)
add_subdirectory(test)

add_executable(tester main.c)
target_link_libraries(tester resource_manager)

install(TARGETS resource_manager
        LIBRARY DESTINATION /usr/local/lib CONFIGURATIONS Release
        PUBLIC_HEADER DESTINATION /usr/local/include CONFIGURATIONS Release)