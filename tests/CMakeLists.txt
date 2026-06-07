# Replace backslashes with forward slashes to prevent C++ escape character issues on Windows
string(REPLACE "\\" "/" SAFE_PROJECT_SOURCE_DIR "${PROJECT_SOURCE_DIR}")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/test_config.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test_config.hpp"
)

add_executable(test_http test_http.cpp)
target_link_libraries(test_http PRIVATE cpphttp::cpphttp GTest::gtest_main)
target_include_directories(test_http PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME HttpTests COMMAND test_http)
