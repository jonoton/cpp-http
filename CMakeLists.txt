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

find_package(ZLIB QUIET)
if(NOT ZLIB_FOUND)
    message(STATUS "ZLIB not found, downloading via FetchContent...")
    include(FetchContent)
    FetchContent_Declare(
      zlib
      URL https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz
    )
    FetchContent_MakeAvailable(zlib)
    
    if(TARGET zlibstatic)
        add_library(ZLIB::ZLIB ALIAS zlibstatic)
        target_include_directories(zlibstatic PUBLIC 
            $<BUILD_INTERFACE:${zlib_SOURCE_DIR}> 
            $<BUILD_INTERFACE:${zlib_BINARY_DIR}>
        )
    elseif(TARGET zlib)
        add_library(ZLIB::ZLIB ALIAS zlib)
        target_include_directories(zlib PUBLIC 
            $<BUILD_INTERFACE:${zlib_SOURCE_DIR}> 
            $<BUILD_INTERFACE:${zlib_BINARY_DIR}>
        )
    endif()
endif()

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

if(IS_TOP_LEVEL)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

# Option to build examples
option(CPPHTTP_BUILD_EXAMPLES "Build the example programs" ${IS_TOP_LEVEL})

if(CPPHTTP_BUILD_EXAMPLES)
    add_executable(websocket_example examples/websocket_example.cpp)
    target_link_libraries(websocket_example PRIVATE cpphttp::cpphttp)
    
    add_executable(spa_example examples/spa_example.cpp)
    target_link_libraries(spa_example PRIVATE cpphttp::cpphttp)
    
    add_executable(download_example examples/download_example.cpp)
    target_link_libraries(download_example PRIVATE cpphttp::cpphttp)
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


