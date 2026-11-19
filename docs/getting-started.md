---
layout: default
title: Getting Started
description: System requirements, CMake integration, manual compilation, and creating your first server and client with cpp-http.
---

# Getting Started with cpp-http

Welcome to `cpp-http`! This guide walks you through system requirements, CMake integration with all supported build options and dependencies, creating your first server and client, and building and running your application.

---

## Table of Contents

1. [System Requirements](#1-system-requirements)
2. [CMake Integration](#2-cmake-integration)
3. [Your First HTTP Server](#3-your-first-http-server)
4. [Your First HTTP Client](#4-your-first-http-client)
5. [Build and Run](#5-build-and-run)

---

## 1. System Requirements

`cpp-http` is written in modern C++17 and is designed to be cross-platform (Linux, macOS, and Windows):

- **C++ Compiler:** Any standard C++17 compiler:
  - GCC 9+ (Linux)
  - Clang 10+ (Linux/macOS)
  - Microsoft Visual Studio 2019+ / MSVC 19.20+ (Windows)
- **CMake:** Version 3.14 or higher (needed for `FetchContent_MakeAvailable`).
- **Operating System Libraries:**
  - **Linux / macOS:** POSIX Threads (`-pthread`) and OpenSSL (for TLS/HTTPS).
  - **Windows:** Microsoft Sockets (`Ws2_32.lib`) and Crypt32 (`Crypt32.lib`), linked automatically by the underlying transport layer. MSVC uses `/bigobj` compile flag.

---

## 2. CMake Integration

The recommended method to integrate `cpp-http` into your project is using CMake's standard `FetchContent` module.

### Basic `FetchContent` Setup

Add the following snippet to your `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.14)
project(my_http_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Declare and fetch cpp-http
FetchContent_Declare(
  cpphttp
  GIT_REPOSITORY https://github.com/jonoton/cpp-http.git
  GIT_TAG main
)
FetchContent_MakeAvailable(cpphttp)

# Add your application
add_executable(my_server server.cpp)
target_link_libraries(my_server PRIVATE cpphttp::cpphttp)

add_executable(my_client client.cpp)
target_link_libraries(my_client PRIVATE cpphttp::cpphttp)
```

By linking `cpphttp::cpphttp` (an interface target), CMake automatically configures:
- Header include search paths for `cpphttp.hpp`.
- Sibling repository dependencies (`cpp-tcpnet`, `cpp-pubsub`, `cpp-asyncworker`).
- Transport and compression libraries (`ZLIB`, `Brotli`, `zstd`, and optionally `cpp-quic`).

---

### CMake Build Options

`cpp-http` provides several CMake options to customize dependencies, protocols, and targets:

| CMake Option | Default | Description |
|:---|:---:|:---|
| `CPPHTTP_ENABLE_QUIC` | `ON` | Enables HTTP/3 (HTTP-over-QUIC) support via `cpp-quic`. Automatically fetches `cpp-quic` and defines `CPPHTTP_ENABLE_QUIC`. Set to `OFF` for pure TCP/HTTPS operation. |
| `CPPHTTP_TCPNET_TAG` | `"main"` | Git branch, tag, or commit hash to fetch for the `cpp-tcpnet` networking engine. |
| `CPPHTTP_QUIC_TAG` | `"main"` | Git branch, tag, or commit hash to fetch for the `cpp-quic` transport dependency (when QUIC is enabled). |
| `CPPHTTP_BUILD_EXAMPLES`| `ON` *(top-level)* | Builds example programs (`websocket_example`, `spa_example`, `download_example`, `quic_http_server`, `quic_http_client`, `protocol_throughput_benchmark`). |
| `CPPHTTP_BUILD_TESTS` | `ON` *(top-level)* | Fetches Google Test and compiles the unit test runner (`test_http`) and saturation server (`benchmark_server`). |

### Disabling QUIC (Pure TCP / HTTPS Mode)

If your application only needs standard HTTP/1.1 and WebSockets over TCP, you can disable HTTP/3 to avoid downloading the `cpp-quic` dependency:

```cmake
# Place before FetchContent_MakeAvailable(cpphttp)
set(CPPHTTP_ENABLE_QUIC OFF CACHE BOOL "Disable QUIC/HTTP-3 support" FORCE)
```

> **Note:** When `CPPHTTP_ENABLE_QUIC=OFF`, the library compiles in pure TCP mode. You will still have full access to `HttpServer`, `HttpClient`, and `WebSocketClient`, without pulling in UDP/QUIC libraries.

---

### Dependency Resolution

`cpp-http` handles compression and transport dependencies transparently:

1. **`cpp-tcpnet` & OpenSSL:** Fetched automatically; enables asynchronous TCP sockets, polling loops, and TLS encryption.
2. **`ZLIB`:** Checks for system ZLIB via `find_package(ZLIB QUIET)`. If not found, CMake automatically downloads and builds `zlib-1.4` as a static library via `FetchContent`.
3. **`Brotli`:** Checks for system Brotli via `find_package(Brotli QUIET)`. If not found, downloads and builds `google/brotli v1.2.1` (`brotlienc`, `brotlidec`, `brotlicommon`).
4. **`zstd`:** Checks for system zstd via `find_package(zstd QUIET)`. If not found, downloads and builds `facebook/zstd v1.5.7` (`libzstd_static`).
5. **`cpp-quic`:** Fetched automatically when `CPPHTTP_ENABLE_QUIC=ON`.

---

### Manual Compilation (Without CMake)

If you compile without CMake, clone `cpp-http` and its sibling repositories (`cpp-tcpnet`, `cpp-pubsub`, `cpp-asyncworker`) into your include path and link the required system libraries:

```bash
# Compilation on Linux / macOS using g++:
g++ -std=c++17 -O3 -pthread \
    -DCPPTCPNET_SSL_SUPPORT \
    -I/path/to/cpp-http \
    -I/path/to/cpp-tcpnet \
    -I/path/to/cpp-pubsub \
    -I/path/to/cpp-asyncworker \
    server.cpp -o server \
    -lssl -lcrypto -lz -lbrotlienc -lbrotlidec -lbrotlicommon -lzstd
```

> **Note:** The command above builds HTTP/1.1 and WebSocket over TCP only. Enabling HTTP/3 (HTTP-over-QUIC) additionally requires defining `-DCPPHTTP_ENABLE_QUIC`, providing include paths for `cpp-quic` and its `cpp-udpnet` dependency, and linking the `cpp-quic` library and its TLS dependencies (OpenSSL or LibreSSL), as configured in `CMakeLists.txt`. Using CMake with `FetchContent` is recommended to resolve these transitive dependencies automatically.

---

## 3. Your First HTTP Server

Create a file named `server.cpp`:

```cpp
#include "cpphttp.hpp"
#include <iostream>

int main() {
    // 1. Instantiate server listening on port 8080 (binds 0.0.0.0 by default)
    cpphttp::HttpServer server(8080);

    // 2. Define a root GET endpoint returning HTML
    server.Get("/", [](const cpphttp::HttpRequest& req) {
        return cpphttp::HttpResponse::Html("<h1>Hello from cpp-http!</h1>");
    });

    // 3. Define a JSON API route with a dynamic path parameter (:id)
    server.Get("/api/items/:id", [](const cpphttp::HttpRequest& req) {
        std::string item_id = req.path_params.at("id");
        return cpphttp::HttpResponse::Json(
            "{\"item_id\":\"" + item_id + "\",\"status\":\"available\"}");
    });

    // 4. Define a POST endpoint that reads client body
    server.Post("/api/echo", [](const cpphttp::HttpRequest& req) {
        cpphttp::HttpResponse res;
        res.status_code = 200;
        res.headers["Content-Type"] = "text/plain";
        res.body = "Received payload: " + req.body;
        return res;
    });

    // 5. Start listening asynchronously in the background
    try {
        server.Start();
        std::cout << "Server running on http://localhost:8080.\n"
                  << "Press Enter to stop.\n";
        std::cin.get();
        server.Stop();
    } catch (const std::exception& e) {
        std::cerr << "Failed to start server: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
```

---

## 4. Your First HTTP Client

Create a file named `client.cpp`:

```cpp
#include "cpphttp.hpp"
#include <iostream>
#include <future>

int main() {
    // 1. Construct client pointing to server host and port
    cpphttp::HttpClient client("127.0.0.1", 8080);
    client.SetTimeout(std::chrono::seconds(5)); // 5-second timeout

    try {
        // 2. Synchronous (blocking) GET request
        std::cout << "[Sync GET] Requesting /api/items/101...\n";
        cpphttp::HttpResponse sync_res = client.Get("/api/items/101");
        std::cout << "Status: " << sync_res.status_code << " "
                  << sync_res.status_message << "\n";
        std::cout << "Response Body: " << sync_res.body << "\n\n";

        // 3. Asynchronous (non-blocking) POST request returning std::future
        std::cout << "[Async POST] Submitting payload to /api/echo...\n";
        std::future<cpphttp::HttpResponse> future =
            client.PostAsync("/api/echo", "Hello Server!");

        // ... do other work while the request runs in the background ...

        cpphttp::HttpResponse async_res = future.get(); // Waits for completion
        std::cout << "Status: " << async_res.status_code << "\n";
        std::cout << "Response Body: " << async_res.body << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Request failed: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
```

---

## 5. Build and Run

Configure and build your application with CMake:

```bash
# Generate build configuration
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Build executables
cmake --build build

# Run the server in one terminal
./build/my_server

# In another terminal, run your client (or test with curl)
./build/my_client
curl -i http://localhost:8080/api/items/999
```

> **Tip:** You can bind to a specific network interface (e.g. `127.0.0.1` for local development) by passing the IP address as the second argument to `HttpServer`: `cpphttp::HttpServer server(8080, "127.0.0.1");`.

---

<div class="page-nav">
  <a href="./index.html" class="page-nav-link">
    <span class="page-nav-subtitle">Previous Section</span>
    <span class="page-nav-title">&larr; Overview</span>
  </a>
  <a href="./basic-usage.html" class="page-nav-link page-nav-next">
    <span class="page-nav-subtitle">Next Section</span>
    <span class="page-nav-title">Basic Usage &rarr;</span>
  </a>
</div>
