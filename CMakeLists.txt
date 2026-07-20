cmake_minimum_required(VERSION 3.10)
project(cpphttp LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/bigobj)
endif()

# Force dependencies in this directory scope and below to build as static libraries
set(BUILD_SHARED_LIBS OFF)

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

option(CPPHTTP_ENABLE_QUIC "Enable HTTP over QUIC using cpp-quic" ON)
if(CPPHTTP_ENABLE_QUIC)
    add_compile_definitions(CPPHTTP_ENABLE_QUIC)
    set(CPPHTTP_QUIC_TAG "main" CACHE STRING "Git tag/branch to use for cpp-quic dependency")
    FetchContent_Declare(
      cppquic
      GIT_REPOSITORY https://github.com/jonoton/cpp-quic.git
      GIT_TAG ${CPPHTTP_QUIC_TAG}
    )
    FetchContent_MakeAvailable(cppquic)
endif()

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

# Brotli
find_package(Brotli QUIET)
if(NOT Brotli_FOUND)
    message(STATUS "Brotli not found, downloading via FetchContent...")
    include(FetchContent)
    FetchContent_Declare(
      brotli
      URL https://github.com/google/brotli/archive/refs/tags/v1.1.0.tar.gz
    )
    set(BROTLI_BUILD_TOOLS OFF CACHE BOOL "Build Brotli command line utility" FORCE)
    set(BROTLI_DISABLE_TESTS ON CACHE BOOL "Disable Brotli tests" FORCE)
    FetchContent_MakeAvailable(brotli)
endif()

# Zstandard (zstd)
find_package(zstd QUIET)
if(NOT zstd_FOUND)
    message(STATUS "zstd not found, downloading via FetchContent...")
    include(FetchContent)
    FetchContent_Declare(
      zstd
      URL https://github.com/facebook/zstd/releases/download/v1.5.6/zstd-1.5.6.tar.gz
      SOURCE_SUBDIR "build/cmake"
    )
    set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "Build programs" FORCE)
    set(ZSTD_BUILD_TESTS OFF CACHE BOOL "Build tests" FORCE)
    set(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "Build contrib" FORCE)
    set(ZSTD_BUILD_SHARED OFF CACHE BOOL "Build shared library" FORCE)
    set(ZSTD_BUILD_STATIC ON CACHE BOOL "Build static library" FORCE)
    FetchContent_MakeAvailable(zstd)
endif()

# Define as an interface library (header-only)
add_library(cpphttp INTERFACE)
add_library(cpphttp::cpphttp ALIAS cpphttp)

target_include_directories(cpphttp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(cpphttp INTERFACE cxx_std_17)

target_link_libraries(cpphttp INTERFACE cpptcpnet::cpptcpnet ZLIB::ZLIB)
if(CPPHTTP_ENABLE_QUIC)
    target_link_libraries(cpphttp INTERFACE cppquic::cppquic)
endif()

if(TARGET brotlienc)
    target_link_libraries(cpphttp INTERFACE brotlienc brotlidec brotlicommon)
    target_include_directories(cpphttp INTERFACE $<BUILD_INTERFACE:${brotli_SOURCE_DIR}/c/include>)
else()
    target_link_libraries(cpphttp INTERFACE Brotli::brotlienc Brotli::brotlidec Brotli::brotlicommon)
endif()

if(TARGET libzstd_static)
    target_link_libraries(cpphttp INTERFACE libzstd_static)
    target_include_directories(cpphttp INTERFACE $<BUILD_INTERFACE:${zstd_SOURCE_DIR}/lib>)
else()
    target_link_libraries(cpphttp INTERFACE zstd::libzstd_static)
endif()

# Determine if this is the top-level project
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(CPPHTTP_IS_TOP_LEVEL ON)
else()
    set(CPPHTTP_IS_TOP_LEVEL OFF)
endif()

if(CPPHTTP_IS_TOP_LEVEL)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

# Option to build examples
option(CPPHTTP_BUILD_EXAMPLES "Build the example programs" ${CPPHTTP_IS_TOP_LEVEL})

if(CPPHTTP_BUILD_EXAMPLES)
    # Replace backslashes with forward slashes to prevent C++ escape character issues on Windows
    string(REPLACE "\\" "/" SAFE_PROJECT_SOURCE_DIR "${PROJECT_SOURCE_DIR}")

    add_executable(websocket_example examples/websocket_example.cpp)
    target_link_libraries(websocket_example PRIVATE cpphttp::cpphttp)
    target_compile_definitions(websocket_example PRIVATE "CPPHTTP_CERTS_DIR=\"${SAFE_PROJECT_SOURCE_DIR}/certs\"")
    
    add_executable(spa_example examples/spa_example.cpp)
    target_link_libraries(spa_example PRIVATE cpphttp::cpphttp)
    
    add_executable(download_example examples/download_example.cpp)
    target_link_libraries(download_example PRIVATE cpphttp::cpphttp)
    
    if(CPPHTTP_ENABLE_QUIC)
        add_executable(quic_http_server examples/quic_http_server.cpp)
        target_link_libraries(quic_http_server PRIVATE cpphttp::cpphttp)
        target_compile_definitions(quic_http_server PRIVATE "CPPHTTP_CERTS_DIR=\"${SAFE_PROJECT_SOURCE_DIR}/certs\"")
        
        add_executable(quic_http_client examples/quic_http_client.cpp)
        target_link_libraries(quic_http_client PRIVATE cpphttp::cpphttp)
    endif()
endif()

option(CPPHTTP_BUILD_TESTS "Build the unit tests" ${CPPHTTP_IS_TOP_LEVEL})

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


