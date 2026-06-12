# cpp-http

`cpp-http` is a lightweight, high-performance, **header-only C++17 HTTP/1.1** server and client library. Built on top of the asynchronous, non-blocking network engine `cpp-tcpnet`, it utilizes a multi-threaded worker pool with **session-affinity ordering** to handle connections concurrently without blocking your main application logic.

---

## Key Features

- **Asynchronous HTTP/1.1 Server:** Background event-loop processing with session-affinity worker threads — multiple connections are handled in parallel while per-connection packet ordering is guaranteed.
- **Flexible HTTP Client:** Supports both **synchronous (blocking)** and **asynchronous (non-blocking)** request methods (`GetAsync`, `PostAsync`, etc. returning `std::future`). Supports automatic redirect following with POST-to-GET method downgrade.
- **Protocol Compliance:** Supports **Chunked Transfer Encoding** (requests/responses), keep-alive connection pipelining, and HTTP/1.0 EOF-delimited bodies.
- **HTTP Streaming:** Server-side `AddStreamRoute` for incremental chunked-upload handling; client-side `GetStream`, `PostStream`, `PostStreamAsync`, and `PutStreamAsync` for streaming large request/response bodies without buffering the entire payload in memory.
- **Dynamic Routing:** Pattern matching for route variables (e.g. `/users/:id/profile`) and wildcards (e.g. `/assets/*`).
- **Static Files & SPA Support:** Serve local directories via `StaticDir` with automatic MIME-type mapping, directory indices, HTTP Range request support (`206 Partial Content` for video/audio seeking), and Single Page Application (SPA) routing fallback.
- **Header Normalization & Multi-Headers:** Built-in case-insensitive header matching and multi-header mapping (e.g. for multiple `Set-Cookie` entries).
- **Query Parameter Normalization:** Automatic URL percent decoding and query parameter parsing.
- **Middleware Pipeline:** Chaining pre-routing hooks to inspect/modify requests or short-circuit responses.
- **CORS Middleware:** Built-in configurable Cross-Origin Resource Sharing (CORS) middleware to manage preflight requests and response headers.
- **Rate Limiting:** Built-in IP-based rate limiting middleware with sliding window duration and automatic `Retry-After` headers.
- **Response Caching:** Lightweight cache wrapper for `GET` route handlers with configurable cache duration and query parameter normalization.
- **Reliability & DoS Protection:** Exception-safe parsing, configurable max header block limits, and strict payload/body size limitations.
- **WebSocket Support:** Fully integrated server-side WebSocket routing and client-side `WebSocketClient` with fragmentation, continuation, and ping/pong keep-alives.
- **SSL/TLS Support:** Out-of-the-box HTTPS, WSS, and client-side SSL support through OpenSSL.

---

## Quick Start (HTTP Server)

```cpp
#include "cpphttp.hpp"
#include <iostream>

int main() {
    // 1. Create a server listening on port 8080 (optionally specify bind IP, e.g., "127.0.0.1")
    cpphttp::HttpServer server(8080);

    // 2. Register global middleware for logging
    server.Use([](cpphttp::HttpRequest &req, cpphttp::HttpResponse &res) {
        std::cout << "[HTTP] Received request: " << req.method << " " << req.path << "\n";
        return true; // continue route matching
    });

    // 3. Register rate limiting: limit each client IP to at most 100 requests every 60 seconds
    server.Use(cpphttp::RateLimiter(100, std::chrono::seconds(60)));

    // Create a response cache with a default 10-second TTL
    cpphttp::ResponseCache cache(std::chrono::seconds(10));

    // 4. Register route with dynamic parameters wrapped with response caching
    server.Get("/users/:id/profile", cache.Wrap([](const cpphttp::HttpRequest &req) {
        std::string user_id = req.path_params.at("id");
        std::string token = req.query_params.count("token") ? req.query_params.at("token") : "none";
        
        std::string body = "{\"user_id\":\"" + user_id + "\",\"token\":\"" + token + "\"}";
        return cpphttp::HttpResponse::Json(body);
    }));

    // 5. Register wildcard static asset routing
    server.Get("/assets/*", [](const cpphttp::HttpRequest &req) {
        std::string file_path = req.path_params.at("*");
        return cpphttp::HttpResponse::Plain("Requested asset: " + file_path);
    });

    // 6. Start the server
    try {
        server.Start();
        std::cout << "Server running. Press Enter to exit.\n";
        std::cin.get();
        server.Stop();
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
    return 0;
}
```

---

## Quick Start (HTTP Client)

```cpp
#include "cpphttp.hpp"
#include <iostream>
#include <future>

int main() {
    try {
        // Create client pointing to host and port
        cpphttp::HttpClient client("127.0.0.1", 8080);
        client.SetTimeout(std::chrono::milliseconds(5000)); // 5-second timeout
        client.SetMaxBodySize(10 * 1024 * 1024);            // limit responses to 10MB

        // Perform blocking GET request
        auto res = client.Get("/users/456/profile?token=secret123");
        std::cout << "Response code: " << res.status_code << "\n";
        std::cout << "Content-Type: " << res.headers.at("Content-Type") << "\n";
        std::cout << "Body: " << res.body << "\n";

        // Perform asynchronous POST request returning a std::future
        std::future<cpphttp::HttpResponse> future_res = client.PostAsync("/submit", "payload string");
        
        // ... do other work concurrently ...

        auto async_res = future_res.get(); // Blocks until response is ready
        std::cout << "Async POST Response code: " << async_res.status_code << "\n";
    } catch (const std::exception &e) {
        std::cerr << "Client error: " << e.what() << "\n";
    }
    return 0;
}
```

---

## Quick Start (WebSocket Client)

```cpp
#include "cpphttp.hpp"
#include <iostream>
#include <thread>

int main() {
    cpphttp::WebSocketClient client;

    // Register callbacks
    client.OnOpen([]() {
        std::cout << "WebSocket Connected!\n";
    });
    client.OnMessage([](const std::string &msg) {
        std::cout << "Received message: " << msg << "\n";
    });
    client.OnClose([](uint16_t code, const std::string &reason) {
        std::cout << "Connection closed: " << code << " (" << reason << ")\n";
    });

    // Connect to a server
    if (client.Connect("ws://127.0.0.1:8080/ws")) {
        client.Send("Hello WebSocket!");
        
        // Let it run briefly to receive response
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        client.Close();
    } else {
        std::cerr << "Failed to connect to WebSocket server\n";
    }
    return 0;
}
```

---

## Integration

### CMake (Recommended)
Add `cpp-http` using CMake `FetchContent`:

```cmake
include(FetchContent)

FetchContent_Declare(
  cpphttp
  GIT_REPOSITORY https://github.com/jonoton/cpp-http.git
  GIT_TAG main
)
FetchContent_MakeAvailable(cpphttp)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE cpphttp::cpphttp)
```

---

## Directory Overview

- [cpphttp.hpp](file:///workspaces/cpp-http/cpphttp.hpp): The complete single-header HTTP server and client library.
- [tests/](file:///workspaces/cpp-http/tests): Testing suite using Google Test.
- [examples/](file:///workspaces/cpp-http/examples): Ready-to-run HTTP/HTTPS/WebSocket server demonstration.
- [docs/](file:///workspaces/cpp-http/docs): Fully comprehensive markdown documentation.
