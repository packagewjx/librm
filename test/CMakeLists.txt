add_subdirectory(googletest)
include_directories(googletest/googletest/include googletest/googlemock/include)

add_executable(tests pqos_utils.cpp utils.cpp process_monitor.cpp control_scheme.cpp $<TARGET_OBJECTS:utils>)
target_link_libraries(tests resource_manager gtest gtest_main)