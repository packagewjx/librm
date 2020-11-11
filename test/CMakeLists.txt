add_subdirectory(googletest)
include_directories(googletest/googletest/include googletest/googlemock/include)

add_executable(tests pqos_utils.cpp utils.cpp)
target_link_libraries(tests utils gtest gtest_main)