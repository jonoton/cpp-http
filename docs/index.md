---
layout: default
---

# cpp-http

`cpp-http` is a lightweight, high-performance, header-only C++17 HTTP/1.1 server and client library. It is built on top of `cpp-tcpnet`, leveraging its asynchronous, non-blocking TCP engine and a multi-threaded worker pool with **session-affinity ordering** to parse and route HTTP requests concurrently without stalling the main application thread.

### Key Features
- **Built on cpp-tcpnet:** Leverages the robust multi-threaded networking foundation of `cpp-tcpnet` with session-affinity worker threads for guaranteed per-connection ordering.
- **Header-Only:** Drop-in C++17 library. Zero compilation required to integrate.
- **Asynchronous Event Loop:** Handles high numbers of concurrent client connections in the background with parallel throughput across connections.
- **HTTP Streaming:** Server-side `AddStreamRoute` for incremental chunked-upload processing; client-side `GetStream`, `PostStream`, `PostStreamAsync`, and `PutStreamAsync` for streaming large payloads without buffering in memory.
- **Dynamic Routing & Path Parameters:** Support route variables (e.g. `/users/:id`) and wildcards (e.g. `/static/*`).
- **Static Files & SPA Support:** Serve local directories via `StaticDir` with automatic MIME-type mapping, directory indices, HTTP Range request support (`206 Partial Content` for video/audio seeking), and Single Page Application (SPA) routing fallback.
- **Header Normalization & Multi-Headers:** Built-in case-insensitive header matching and multi-header mapping (e.g. for multiple `Set-Cookie` entries).
- **Query Parameter & URL Percent Decoding:** Automatically decodes and parses query variables and route parameters.
- **Middleware Pipeline:** Chain processing logic before handlers run to validate or modify requests.
- **CORS Middleware:** Built-in configurable Cross-Origin Resource Sharing (CORS) middleware to manage preflight requests and response headers.
- **Rate Limiting Middleware:** Built-in IP-based rate limiting with sliding time windows and automatic `Retry-After` headers.
- **Response Caching Wrapper:** Lightweight route handler caching for `GET` requests with configurable cache duration.
- **HTTP Client Redirect Handling:** Client supports automatic redirect following with standard POST-to-GET method downgrade.
- **WebSocket Client & Server:** Fully integrated server-side WebSocket routing and client-side `WebSocketClient` with fragmentation, continuation, and ping/pong keep-alives.
- **Ergonomic Response Builders:** Quickly construct structured responses (`Json`, `Html`, `Plain`, `Redirect`) with status code resolution.
- **Connection Lifecycle Management:** Explicitly handles `Connection: close` headers for clean socket cleanups.
- **Defragmentation & DoS Limits:** Configurable max request header boundaries to prevent memory exhaust exploits.
- **Thread-Pooled Routing:** Workloads are parsed and dispatched concurrently via `cpp-tcpnet` session-affinity worker threads.

## Documentation Pages

Welcome to the `cpp-http` documentation! Please follow the guide below to learn how to integrate and use the library:

1. **[Getting Started](./getting-started.html)**
   Learn how to integrate `cpp-http` into your project via CMake or direct inclusion.
   
2. **[Basic Usage](./basic-usage.html)**
   Learn how to start the HTTP server and register basic GET/POST endpoints.

3. **[Advanced Usage](./advanced-usage.html)**
   Explore advanced request processing, customized response structures, and HTTP header management.

4. **[Performance & Tuning](./performance-metrics.html)**
   Learn about connection scaling limits, tweaking HTTP buffer limits, and optimizing TCP options.

5. **[Architecture & Examples](./architecture-and-examples.html)**
   Understand the internal architecture of `cpp-http` and how it maps raw TCP buffers to routed HTTP structures.

---
[Start Reading: Getting Started >](./getting-started.html)
