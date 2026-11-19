# cpp-http

> Header-only C++17 **HTTP/1.1**, **HTTP/3 (QUIC)**, and **WebSocket** server and client library.

![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)
![Header-only](https://img.shields.io/badge/header--only-cpphttp.hpp-brightgreen)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
![HTTP/1.1](https://img.shields.io/badge/HTTP-1.1-orange)
![HTTP/3](https://img.shields.io/badge/HTTP-3%20%28QUIC%29-purple)
![WebSocket](https://img.shields.io/badge/WebSocket-RFC%206455-teal)

[Docs](https://jonoton.github.io/cpp-http/) ·
[Getting Started](https://jonoton.github.io/cpp-http/getting-started.html) ·
[Basic Usage](https://jonoton.github.io/cpp-http/basic-usage.html) ·
[Advanced Usage](https://jonoton.github.io/cpp-http/advanced-usage.html) ·
[Architecture & Examples](https://jonoton.github.io/cpp-http/architecture-and-examples.html) ·
[Performance Metrics](https://jonoton.github.io/cpp-http/performance-metrics.html)

## Why cpp-http?

- **Single-header drop-in:** Include `cpphttp.hpp` and link `cpphttp::cpphttp` via CMake `FetchContent`; transport, TLS, and compression dependencies resolve automatically.
- **One API across transports:** Write routes once and serve TCP (`HttpServer`), QUIC/HTTP/3 (`QuicHttpServer`), or both on one port (`HybridHttpServer`) with `Alt-Svc` upgrade advertisement.
- **Session-affinity concurrency:** Non-blocking I/O (`cpp-tcpnet` / `cpp-quic`) dispatches work across a worker pool so connections scale in parallel while per-session ordering is preserved.
- **Production defaults out of the box:** Transparent `zstd`/`br`/`gzip` negotiation, streaming I/O, static/SPA serving with `Range` support, header/body size limits, and peer TLS verification enabled by default.

## Features

| Area | Highlights |
| --- | --- |
| **Server** | `HttpServer`, `QuicHttpServer`, `HybridHttpServer`; `:param` & `*` routing; `StaticDir` + SPA + `Range`; streaming uploads |
| **Client** | Sync & async `HttpClient` and `HttpClientImpl<QuicClientTransport>`; streaming I/O; safe redirects |
| **Protocols** | HTTP/1.1 (keep-alive, chunked), HTTP/3 over QUIC (`Alt-Svc`), WebSockets (RFC 6455), TLS (LibreSSL or OpenSSL) |
| **Middleware** | Global & prefix `Use`, `Cors`, IP `RateLimiter`, and TTL `ResponseCache` |
| **Compression** | Prioritized `zstd` > `br` > `gzip` negotiation and automatic client decompression |
| **Security** | Header/body limits, canonical path-traversal checks, `ClientTlsConfig` (`verify_peer = true`), opt-in proxy headers |

## Quick Start

```cpp
#include "cpphttp.hpp"
#include <chrono>
#include <iostream>

int main() {
  cpphttp::HttpServer server(8080, "127.0.0.1");
  server.Get("/hello/:name", [](const cpphttp::HttpRequest &req) {
    auto name = req.path_params.at("name");
    return cpphttp::HttpResponse::Json("{\"hello\":\"" + name + "\"}");
  });
  server.Start();

  cpphttp::HttpClient client("127.0.0.1", 8080);
  client.SetTimeout(std::chrono::seconds(5));

  auto res = client.Get("/hello/world");
  std::cout << res.status_code << " " << res.body << "\n";

  auto fut = client.GetAsync("/hello/async");
  std::cout << "Async: " << fut.get().body << "\n";

  server.Stop();
  return 0;
}
```

See [Basic Usage](https://jonoton.github.io/cpp-http/basic-usage.html) and [Advanced Usage](https://jonoton.github.io/cpp-http/advanced-usage.html) for middleware, streaming, WebSockets, TLS, and HTTP/3.

## Installation (CMake FetchContent)

Requires a C++17 compiler and CMake 3.14+. TLS comes from LibreSSL, built from source by default via `cpp-tcpnet` (set `CPPTCPNET_USE_LIBRESSL=OFF` to use system OpenSSL). Link `cpphttp::cpphttp`:

```cmake
include(FetchContent)

FetchContent_Declare(
  cpphttp
  GIT_REPOSITORY https://github.com/jonoton/cpp-http.git
  GIT_TAG main
)
# Optional: disable HTTP/3 (cpp-quic) for TCP/WebSocket-only builds
# set(CPPHTTP_ENABLE_QUIC OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cpphttp)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE cpphttp::cpphttp)
```

| Option | Default | Description |
| --- | --- | --- |
| `CPPHTTP_ENABLE_QUIC` | `ON` | Enable HTTP/3 via `cpp-quic` (defines `CPPHTTP_ENABLE_QUIC`) |
| `CPPHTTP_TCPNET_TAG` | `"main"` | Git tag/branch for `cpp-tcpnet` |
| `CPPHTTP_QUIC_TAG` | `"main"` | Git tag/branch for `cpp-quic` |
| `CPPHTTP_BUILD_EXAMPLES` | `ON`\* | Build binaries in `examples/` |
| `CPPHTTP_BUILD_TESTS` | `ON`\* | Build `test_http` (`HttpTests`) and `benchmark_server` |

\* Defaults to `ON` for top-level builds and `OFF` when consumed as a subproject. See [Getting Started](https://jonoton.github.io/cpp-http/getting-started.html) for automatic dependency resolution (`zlib`, `Brotli`, `zstd`) and non-CMake builds.

## Build & Test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
ctest --test-dir build --output-on-failure
```

HTTP/3 interop scripts live in [`tests/test_quic_curl.sh`](tests/test_quic_curl.sh) and [`tests/test_quic_chrome.sh`](tests/test_quic_chrome.sh); benchmark instructions and results live in [Performance Metrics](https://jonoton.github.io/cpp-http/performance-metrics.html).

## Examples

| Target | QUIC | Summary |
| --- | --- | --- |
| [`download_example`](examples/download_example.cpp) | No | Static file server (`StaticDir`) with streaming & compression UI |
| [`spa_example`](examples/spa_example.cpp) | No | SPA fallback with cookie-based session auth endpoints |
| [`websocket_example`](examples/websocket_example.cpp) | No | HTTPS + secure WebSocket (`wss://`) echo server & browser UI |
| [`quic_http_server`](examples/quic_http_server.cpp) | Yes | Dual-stack HTTPS + HTTP/3 (`HybridHttpServer`) with stats JSON |
| [`quic_http_client`](examples/quic_http_client.cpp) | Yes | HTTP/3 client (`HttpClientImpl<QuicClientTransport>`) |
| [`protocol_throughput_benchmark`](examples/protocol_throughput_benchmark.cpp) | Yes | 20 MB throughput benchmark across HTTPS, HTTP/3, and WSS |

Build and run walkthroughs are in [Architecture & Examples](https://jonoton.github.io/cpp-http/architecture-and-examples.html).

## Documentation

| Guide | Covers |
| --- | --- |
| [Home](https://jonoton.github.io/cpp-http/) | Library overview and feature summary |
| [Getting Started](https://jonoton.github.io/cpp-http/getting-started.html) | Requirements, CMake options, dependencies, manual builds |
| [Basic Usage](https://jonoton.github.io/cpp-http/basic-usage.html) | Routing, params, headers, sync/async client, error handling |
| [Advanced Usage](https://jonoton.github.io/cpp-http/advanced-usage.html) | Middleware, CORS, rate limit, cache, compression, streaming, WS, TLS, HTTP/3 |
| [Architecture & Examples](https://jonoton.github.io/cpp-http/architecture-and-examples.html) | Threading model, event loop, transport stack, example walkthroughs |
| [Performance Metrics](https://jonoton.github.io/cpp-http/performance-metrics.html) | Tuning, SSL/TLS, throughput stats, benchmarks & env vars |

## License

MIT License — see [LICENSE](LICENSE).
