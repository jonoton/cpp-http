---
layout: default
title: Overview
description: High-performance, header-only C++17 HTTP/1.1 and HTTP/3 server and client library.
---

## High-Performance C++17 HTTP/1.1 & HTTP/3

`cpp-http` is a modern, lightweight, **header-only C++17** HTTP/1.1 and HTTP/3 server and client library. Engineered on top of the asynchronous network engines [`cpp-tcpnet`](https://github.com/jonoton/cpp-tcpnet) and [`cpp-quic`](https://github.com/jonoton/cpp-quic), it provides an asynchronous event loop combined with **session-affinity multi-threading** — allowing thousands of concurrent connections to be routed in parallel across worker threads while guaranteeing strict per-connection packet ordering.

Simply drop `cpphttp.hpp` into your project, link CMake target `cpphttp::cpphttp`, and begin building resilient, low-latency microservices, REST APIs, and real-time WebSocket applications.

---

## Quick Teaser

Experience the simplicity of building asynchronous servers and executing non-blocking client requests in modern C++17:

### Minimal HTTP Server
```cpp
#include "cpphttp.hpp"
#include <iostream>

int main() {
    cpphttp::HttpServer server(8080);

    // Route with dynamic path variable returning JSON
    server.Get("/users/:id", [](const cpphttp::HttpRequest& req) {
        std::string id = req.path_params.at("id");
        return cpphttp::HttpResponse::Json(
            "{\"id\":\"" + id + "\",\"status\":\"active\"}");
    });

    server.Start();
    std::cout << "Server listening on http://localhost:8080\n";
    std::cin.get();
    server.Stop();
    return 0;
}
```

### Asynchronous HTTP Client
```cpp
#include "cpphttp.hpp"
#include <iostream>
#include <future>

int main() {
    cpphttp::HttpClient client("127.0.0.1", 8080);

    // Send asynchronous non-blocking GET request
    std::future<cpphttp::HttpResponse> future = client.GetAsync("/users/42");

    // Perform concurrent background work while the request completes...
    cpphttp::HttpResponse res = future.get();
    std::cout << "HTTP " << res.status_code << " -> " << res.body << "\n";
    return 0;
}
```

---

## Features at a Glance

<div class="feature-grid">
  <div class="feature-card">
    <div class="feature-card-header">
      <div class="feature-icon">🚀</div>
      <h3>Core & Transports</h3>
    </div>
    <p>Zero-overhead network abstractions designed for modern protocols.</p>
    <ul>
      <li><strong>Header-Only:</strong> Drop-in C++17 single header (<code>cpphttp.hpp</code>).</li>
      <li><strong>HTTP/1.1 & HTTP/3:</strong> Full HTTP/3 over QUIC support via <code>cpp-quic</code>.</li>
      <li><strong>Hybrid Dual-Stack Server:</strong> Binds TCP (HTTPS) and UDP (QUIC) on the same port with automatic <code>Alt-Svc</code> advertisement.</li>
      <li><strong>SSL/TLS Encryption:</strong> HTTPS and WSS support via OpenSSL.</li>
    </ul>
  </div>

  <div class="feature-card">
    <div class="feature-card-header">
      <div class="feature-icon">⚡</div>
      <h3>Concurrency & Threading</h3>
    </div>
    <p>Multi-threaded scalability with built-in state consistency guarantees.</p>
    <ul>
      <li><strong>Session Affinity:</strong> Connections are pinned to worker threads (<code>session_id % num_threads</code>) to guarantee per-connection ordering without locks.</li>
      <li><strong>Non-Blocking I/O:</strong> OS selectors (<code>poll</code> / <code>WSAPoll</code>) monitor sockets on dedicated background polling threads.</li>
      <li><strong>Asynchronous Client Pool:</strong> Thread pool management for async HTTP requests via <code>cpp-asyncworker</code>.</li>
    </ul>
  </div>

  <div class="feature-card">
    <div class="feature-card-header">
      <div class="feature-icon">🛣️</div>
      <h3>Routing & Semantics</h3>
    </div>
    <p>Clean, declarative routing with built-in normalization.</p>
    <ul>
      <li><strong>Dynamic Parameters:</strong> Named path segments (<code>/users/:id</code>) and wildcards (<code>/static/*</code>).</li>
      <li><strong>Query & URL Parsing:</strong> Automatic percent-decoding and key-value parameter extraction.</li>
      <li><strong>Headers & Cookies:</strong> Case-insensitive header maps and multi-header support (e.g. multiple <code>Set-Cookie</code> entries).</li>
      <li><strong>Static Files & SPA:</strong> Local directory serving with MIME mapping, Range requests (<code>206 Partial Content</code>), and SPA fallback.</li>
    </ul>
  </div>

  <div class="feature-card">
    <div class="feature-card-header">
      <div class="feature-icon">🛡️</div>
      <h3>Middleware & Resiliency</h3>
    </div>
    <p>Modular pipeline architecture for security and traffic control.</p>
    <ul>
      <li><strong>Middleware Pipeline:</strong> Global and path-prefixed pre-routing hooks (<code>server.Use</code>).</li>
      <li><strong>CORS Middleware:</strong> Automated preflight handling and header controls.</li>
      <li><strong>Rate Limiting:</strong> Fixed-window IP rate limiting with automated <code>Retry-After</code> headers.</li>
      <li><strong>Response Caching:</strong> In-memory TTL caching wrapper for idempotent GET requests.</li>
      <li><strong>DoS Protection:</strong> Max header limits, body size enforcement, and idle timeouts.</li>
    </ul>
  </div>

  <div class="feature-card">
    <div class="feature-card-header">
      <div class="feature-icon">📦</div>
      <h3>Streaming & Compression</h3>
    </div>
    <p>Handle large datasets without unbounded memory buffering.</p>
    <ul>
      <li><strong>Transparent Compression:</strong> Priority negotiation for Zstandard (<code>zstd</code>), Brotli (<code>br</code>), and Gzip (<code>gzip</code>).</li>
      <li><strong>Server Streaming:</strong> Incremental chunked-upload handling via <code>PostStream</code> and <code>PutStream</code>.</li>
      <li><strong>Client Streaming:</strong> Non-buffering streaming uploads and downloads via <code>GetStream</code>, <code>PostStream</code>, and <code>PostStreamAsync</code>.</li>
    </ul>
  </div>

  <div class="feature-card">
    <div class="feature-card-header">
      <div class="feature-icon">🔌</div>
      <h3>WebSockets & Real-Time</h3>
    </div>
    <p>Bidirectional full-duplex communication built right into the stack.</p>
    <ul>
      <li><strong>Server WebSocket Route:</strong> Register endpoints via <code>server.WebSocket("/ws", behavior)</code>.</li>
      <li><strong>Client WebSocket:</strong> Event-driven <code>WebSocketClient</code> with <code>OnOpen</code>, <code>OnMessage</code>, and <code>OnClose</code> callbacks.</li>
      <li><strong>Protocol Features:</strong> Automatic ping/pong keep-alives, frame fragmentation, and continuation reassembly.</li>
    </ul>
  </div>
</div>

---

## Where to Go Next

Explore the documentation guides to integrate and harness `cpp-http`:

<div class="next-grid">
  <a href="./getting-started.html" class="next-card">
    <div class="next-card-title">
      <span>1. Getting Started</span>
      <span>&rarr;</span>
    </div>
    <p>Prerequisites, CMake FetchContent integration, compilation flags, and your first server/client.</p>
  </a>

  <a href="./basic-usage.html" class="next-card">
    <div class="next-card-title">
      <span>2. Basic Usage</span>
      <span>&rarr;</span>
    </div>
    <p>Routing syntax, path/query parameters, request/response builders, headers, sync/async clients, and error handling.</p>
  </a>

  <a href="./advanced-usage.html" class="next-card">
    <div class="next-card-title">
      <span>3. Advanced Usage</span>
      <span>&rarr;</span>
    </div>
    <p>Middleware pipelines, CORS, rate limiters, response caching, compression, WebSockets, and HTTP/3 QUIC.</p>
  </a>

  <a href="./architecture-and-examples.html" class="next-card">
    <div class="next-card-title">
      <span>4. Architecture & Examples</span>
      <span>&rarr;</span>
    </div>
    <p>Background event pump, session-affinity worker pool, sibling libraries, and runnable example programs.</p>
  </a>

  <a href="./performance-metrics.html" class="next-card">
    <div class="next-card-title">
      <span>5. Performance & Tuning</span>
      <span>&rarr;</span>
    </div>
    <p>Socket options, connection profiles, thread tuning, benchmark reproduction, and throughput metrics.</p>
  </a>
</div>

<div class="page-nav">
  <div></div>
  <a href="./getting-started.html" class="page-nav-link page-nav-next">
    <span class="page-nav-subtitle">Next Section</span>
    <span class="page-nav-title">Getting Started &rarr;</span>
  </a>
</div>
