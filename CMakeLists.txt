cmake_minimum_required(VERSION 3.10)
project(cpphttp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# Force SSL support in the underlying tcpnet library
set(CPPTCPNET_ENABLE_SSL ON CACHE BOOL "Enable SSL/TLS support using OpenSSL" FORCE)

set(CPPHTTP_TCPNET_TAG "main" CACHE STRING "Git tag/branch to use for cpp-tcpnet dependency")
FetchContent_Declare(
  cpptcpnet
  GIT_REPOSITORY https://github.com/jonoton/cpp-tcpnet.git
  GIT_TAG ${CPPHTTP_TCPNET_TAG}
)
FetchContent_MakeAvailable(cpptcpnet)

find_package(ZLIB REQUIRED)

# Define as an interface library (header-only)
add_library(cpphttp INTERFACE)
add_library(cpphttp::cpphttp ALIAS cpphttp)

target_include_directories(cpphttp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(cpphttp INTERFACE cxx_std_17)

target_link_libraries(cpphttp INTERFACE cpptcpnet::cpptcpnet ZLIB::ZLIB)

# Determine if this is the top-level project
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(IS_TOP_LEVEL ON)
else()
    set(IS_TOP_LEVEL OFF)
endif()

# Option to build examples
option(CPPHTTP_BUILD_EXAMPLES "Build the example programs" ${IS_TOP_LEVEL})

if(CPPHTTP_BUILD_EXAMPLES)
    add_executable(http_example examples/main.cpp)
    target_link_libraries(http_example PRIVATE cpphttp::cpphttp)
endif()

option(CPPHTTP_BUILD_TESTS "Build the unit tests" ${IS_TOP_LEVEL})

if(CPPHTTP_BUILD_TESTS)
    enable_testing()

    include(FetchContent)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_subdirectory(tests)
endif()


