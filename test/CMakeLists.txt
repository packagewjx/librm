add_subdirectory(googletest)
include_directories(googletest/googletest/include googletest/googlemock/include)

add_executable(tests pqos_utils.cpp)
target_link_libraries(tests pqos_utils gtest gtest_main)