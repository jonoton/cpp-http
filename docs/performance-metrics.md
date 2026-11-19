---
layout: default
title: Performance & Tuning
description: Listener tuning, connection profiles, worker pools, throughput metrics, and benchmark reproduction.
---

# Performance, Tuning, and SSL/TLS

`HttpServer` exposes its underlying `cpptcpnet::TcpListener` instance via the `GetListener()` method. This allows you to configure socket options, apply connection profiles, enable SSL/TLS, tune multi-threaded worker pools, and track network performance metrics exactly as you would in a low-level TCP application.

---

## Table of Contents

1. [Accessing the Listener](#1-accessing-the-listener)
2. [SSL/TLS Encryption (HTTPS)](#2-ssltls-encryption-https)
3. [Performance Tuning & Socket Options](#3-performance-tuning--socket-options)
4. [Throughput Tracking & Direct Statistics](#4-throughput-tracking--direct-statistics)
5. [Benchmarking Methodology](#5-benchmarking-methodology)
6. [Measured Performance Results](#6-measured-performance-results)
7. [How to Reproduce Benchmarks](#7-how-to-reproduce-benchmarks)

---

## 1. Accessing the Listener

To query or configure the low-level network layer, call `server.GetListener()`:

```cpp
cpphttp::HttpServer server(8080);

// Get a reference to the underlying TcpListener
cpptcpnet::TcpListener& listener = server.GetListener();
```

---

## 2. SSL/TLS Encryption (HTTPS)

To secure HTTP traffic (HTTPS), configure the underlying TCP listener with SSL certificates before calling `server.Start()`:

```cpp
#ifdef CPPTCPNET_SSL_SUPPORT
    cpptcpnet::TcpListener::SslConfig ssl_config;
    ssl_config.cert_file = "certs/server.crt";
    ssl_config.key_file = "certs/server.key";
    
    // Optional: enable mutual TLS (mTLS)
    ssl_config.require_client_cert = false;

    // Enable SSL on the underlying listener
    server.GetListener().EnableSSL(ssl_config);
#endif
```

---

## 3. Performance Tuning & Socket Options

### Low-Level Socket Options

Fine-tune socket-level settings to optimize HTTP performance for your network conditions:

```cpp
// Disable Nagle's algorithm (lower latency for small responses)
server.GetListener().SetNoDelay(true);

// Set kernel buffer sizes (64KB)
server.GetListener().SetSocketRecvBufferSize(65536);
server.GetListener().SetSocketSendBufferSize(65536);

// Tune TCP Keep-Alives
cpptcpnet::KeepAliveConfig ka;
ka.enabled = true;
ka.idle_secs = 60;
ka.interval_secs = 15;
ka.count = 4;
server.GetListener().SetKeepAliveConfig(ka);
```

### Connection Profiles

Apply pre-tuned presets for specific networking environments:

```cpp
// Apply ReliableLAN profile for local high-speed routing
server.GetListener().SetDefaultConnectionProfile(
    cpptcpnet::ConnectionProfile::ReliableLAN()
);
```

### WebSocket Connection Profiles

You can apply dynamic connection profiles on the fly to individual sessions. When using WebSockets, configure unique keep-alive, idle timeout, and Nagle options via `WebSocketBehavior::connection_profile`, which are applied automatically upon upgrading:

```cpp
cpphttp::WebSocketBehavior ws_behavior;

// Override default WebSocket profile settings (custom defaults in cpphttp:
// no_delay=true, keepalive 60s/15s/4 probes, idle_timeout 1 hour)
ws_behavior.connection_profile.no_delay = true;
ws_behavior.connection_profile.idle_timeout =
    std::chrono::hours(2); // keep open for 2 hours

server.WebSocket("/ws", std::move(ws_behavior));
```

### Configuring Request, Timeout, and Redirect Limits

To protect servers and clients against memory-exhaustion and slow-rate DoS attacks, configure explicit boundaries:

```cpp
// 1. Configure HttpServer
server.SetMaxHeaderSize(16384);          // Up to 16KB headers (default 8KB)
server.SetMaxBodySize(50 * 1024 * 1024); // Reject bodies > 50MB (default 10MB)

// Set connection idle timeout via the listener's connection profile:
auto profile = server.GetListener().GetDefaultConnectionProfile();
profile.idle_timeout = std::chrono::seconds(30); // Close idle connections (30s)
server.GetListener().SetDefaultConnectionProfile(profile);

// 2. Configure HttpClient
cpphttp::HttpClient client("127.0.0.1", 8080);
client.SetTimeout(std::chrono::seconds(5)); // Timeout requests after 5 seconds
client.SetMaxBodySize(10 * 1024 * 1024);    // Reject response bodies > 10MB
client.SetMaxRedirects(10);                 // Set max redirects followed (10)

// 3. Configure WebSocketClient
cpphttp::WebSocketClient ws_client;
ws_client.SetMaxBodySize(20 * 1024 * 1024); // Max frame size (default 10MB)
ws_client.SetCloseTimeout(std::chrono::milliseconds(2000)); // Handshake timeout
```

> **Warning:** Do not use `server.SetIdleTimeout(...)` to configure connection idle timeouts. In `HttpServer`, `SetIdleTimeout` configures the background event-pump wait interval on `shutdown_cv_` when both event queues are empty (default `10ms`), not a client connection timeout. Setting it to a large value (such as 30s) stalls the event pump loop, delaying send completions, graceful disconnects, and chunked file transfer progression. Connection idle timeouts must always be configured via `cpptcpnet::ConnectionProfile::idle_timeout` on `server.GetListener().SetDefaultConnectionProfile(...)`.

### Thread Pool Tuning (HttpServer)

`HttpServer` relies on `cpp-tcpnet`'s internal worker pool to dispatch received data. By default, it uses `std::thread::hardware_concurrency()` worker threads. The worker pool uses **session-affinity hashing** — each connection is pinned to a specific thread (via `session_id % num_threads`), so multiple threads can run in parallel without corrupting per-connection state.

You can tune the server's worker thread count via the underlying listener before calling `Start()`:

```cpp
cpphttp::HttpServer server(8080);

// Use a fixed number of worker threads (default: hardware concurrency)
server.GetListener().SetWorkerThreadCount(4);

server.Start();
```

> **Note:** Because session-affinity is maintained, all thread count values ≥ 1 are safe. Higher counts increase parallelism across simultaneous connections; there is no need to set it to 1 for ordering correctness.

### Thread Pool Tuning (HttpClient)

By default, all instances of `HttpClient` share a global background `cppasyncworker::WorkerPool` for processing asynchronous requests (`GetAsync`, `PostAsync`, etc.). The default pool size is `std::thread::hardware_concurrency()`, falling back to 4 only when concurrency is ≤ 1 (there is NO minimum of 4 if concurrency is greater than 1).

You can customize the size of this global worker pool or pass a custom pool to individual clients:

```cpp
// 1. Customize global thread pool size (must call BEFORE first HttpClient)
cpphttp::SetClientWorkerPoolSize(8);

// 2. Or, construct an HttpClient using a fully custom, independent worker pool
auto my_pool = std::make_shared<cppasyncworker::WorkerPool>(16);
cpphttp::HttpClient client("127.0.0.1", 8080, my_pool);
```

> **Note:** `SetClientWorkerPoolSize(...)` must be called *before* constructing the first `HttpClient` that uses the default pool. Because the default worker pool is a function-local static, subsequent calls to `SetClientWorkerPoolSize` will have no effect on an already-instantiated default pool.

---

## 4. Throughput Tracking & Direct Statistics

### Real-Time Sliding-Window Tracker

Monitor real-time HTTP upload and download rates (bytes/second) by passing the listener's event broker to the `ThroughputTracker` utility:

```cpp
#include "cpphttp.hpp"
#include <iostream>
#include <thread>

int main() {
    cpphttp::HttpServer server(8080);
    server.Start();

    // Track throughput over a 1-second sliding window
    cpptcpnet::ThroughputTracker tracker(
        server.GetListener().GetEventBroker(), 
        std::chrono::seconds(1)
    );

    // Monitor in a background loop
    std::thread([&tracker]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::cout << "Upload rate: "
                      << tracker.GetSendThroughputBytesPerSec() << " B/s | "
                      << "Download rate: "
                      << tracker.GetRecvThroughputBytesPerSec() << " B/s\n";
        }
    }).detach();

    std::cin.get();
    server.Stop();
}
```

### Direct Stats (Atomic Counters)

Query cumulative bytes and active connection count handled by the server:

```cpp
cpptcpnet::ListenerStats stats = server.GetListener().GetStats();

std::cout << "Active HTTP Connections: " << stats.active_connections << "\n"
          << "Total Bytes Transmitted: " << stats.bytes_sent << " bytes\n"
          << "Total Bytes Received:    " << stats.bytes_received << " bytes\n";
```

---

## 5. Benchmarking Methodology

To validate production readiness, throughput, and multi-core scalability under sustained load, `cpp-http` incorporates a multi-tiered benchmarking methodology:

1. **Raw C++ Network Throughput (`protocol_throughput_benchmark`):**
   - Measures raw protocol streaming throughput using a large 20MB payload (`20971520` bytes).
   - Compares HTTP/1.1 (SSL/TCP), HTTP/3 (SSL/QUIC initial handshake and warm connection pass), and WebSocket bidirectional echo streaming.
2. **Closed-Loop Saturation & Scaling (`autocannon`):**
   - Drives continuous back-to-back concurrent connections (`200` concurrent connections, pipelining `1`, `10` second duration) to identify maximum throughput (requests/second), latency percentiles (mean and p99), and multi-core scaling efficiency.
   - Evaluated on both plaintext routing (`GET /`) and CPU-intensive transparent compression (`GET /data` with Gzip).
3. **Open-Loop Scenario Testing (`Artillery.io`):**
   - Fixed arrival rate load testing across multi-step scenarios, query parameter parsing, and WebSocket connections to assess routing overhead, protocol correctness, and latency distributions without saturating CPU.
4. **Resource Footprint Monitoring:**
   - Samples peak and average CPU percentages alongside resident set size (RSS) peak memory footprint.

---

## 6. Measured Performance Results

The following metrics represent actual empirical benchmark measurements conducted on Linux using the automated benchmark suite:

### 1. Raw C++ Protocol Throughput (20MB Payload)

| Protocol / Transport | Duration (s) | Throughput (MB/s) | Notes |
|:---|:---:|:---:|:---|
| **HTTP/1.1 (SSL/TCP)** | 0.204514 s | **97.793 MB/s** | Large payload streaming over persistent TLS |
| **HTTP/3 (SSL/QUIC) Handshake Pass** | 0.383865 s | **52.1016 MB/s** | Initial cold connection establishment + 20MB download |
| **HTTP/3 (SSL/QUIC) Warm Pass** | 0.307438 s | **65.0537 MB/s** | Reused warm QUIC connection (reused warm connection) |
| **WebSocket (SSL/TCP)** | 0.150241 s | **266.239 MB/s** | High-frequency frame echo (Send + Recv) |

---

### 2. Closed-Loop Saturation & Multi-Core Scaling (Autocannon)

Tested with `200` concurrent connections over a `10.01` second saturation window:

#### Baseline Plaintext Throughput (`GET /`)

| Worker Threads | Avg Requests / sec | Latency (Mean / p99) | Scaling Ratio |
| :--- | :---: | :---: | :---: |
| **1 Thread** | 28,139.2 req/s | 6.11 ms / 7 ms | 1.00x *(Baseline)* |
| **8 Threads** | 31,659.2 req/s | 5.53 ms / 10 ms | 1.13x *(I/O bound)* |
| **16 Threads** | 30,582.4 req/s | 5.76 ms / 10 ms | 1.09x *(I/O bound)* |

#### CPU-Bound Transparent Compression (`GET /data` with Gzip)

| Worker Threads | Avg Requests / sec | Latency (Mean / p99) | Scaling Ratio |
| :--- | :---: | :---: | :---: |
| **1 Thread** | 8,391.21 req/s | 22.62 ms / 26 ms | 1.00x *(Baseline)* |
| **8 Threads** | 21,529.6 req/s | 7.77 ms / 16 ms | **2.57x** *(CPU scaling)* |
| **16 Threads** | 23,303.2 req/s | 7.49 ms / 16 ms | **2.78x** *(CPU scaling)* |

> **Note:** For lightweight plaintext endpoints (`GET /`), throughput exceeds 28,000 req/sec on a single thread and quickly becomes bound by the client load generator and single-socket I/O rather than request workers. For CPU-bound workloads like dynamically compressed responses (`GET /data`), session-affinity worker threads scale effectively, achieving a **2.78x** throughput increase.

---

### 3. Resource Footprint & Open-Loop Stability

| Worker Threads | Peak Memory | CPU (Peak / Avg) | HTTP Latency (Mean / p99) |
| :---: | :---: | :---: | :---: |
| **1 Thread** | 29 MB | 29% / 5% | 0.4 ms / 1 ms |
| **8 Threads** | 29 MB | 29% / 5% | 0.5 ms / 1 ms |
| **16 Threads** | 29 MB | 29% / 6% | 0.5 ms / 1 ms |

---

## 7. How to Reproduce Benchmarks

### CMake Benchmark Targets

The repository includes standalone CMake targets for executing load and throughput tests:

1. **`benchmark_server`:** Defined in `tests/CMakeLists.txt` (enabled when `CPPHTTP_BUILD_TESTS=ON`). Builds `tests/benchmark/benchmark_server.cpp`.
2. **`protocol_throughput_benchmark`:** Defined in `CMakeLists.txt` (enabled when `CPPHTTP_BUILD_EXAMPLES=ON` and `CPPHTTP_ENABLE_QUIC=ON`). Builds `examples/protocol_throughput_benchmark.cpp`.

### Building the Targets

Configure and build with CMake in Release mode:

```bash
# Configure with test and example targets enabled
cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCPPHTTP_BUILD_TESTS=ON \
    -DCPPHTTP_BUILD_EXAMPLES=ON \
    -DCPPHTTP_ENABLE_QUIC=ON

# Compile the targets
cmake --build build --config Release
```

### Running the Automated Benchmark Suite

To run the complete automated test harness (including Artillery open-loop, autocannon closed-loop saturation, and protocol throughput tests):

```bash
# Execute the automated benchmark harness on port 8080
./tests/benchmark/run_benchmark.sh 8080
```

### Configurable Environment Variables

The benchmark suite supports environment customization:

```bash
# Benchmark across 1, 4, 8 worker threads with custom connection counts
BENCH_THREADS="1 4 8" \
SAT_CONNECTIONS=100 \
SAT_WORKERS=4 \
SAT_DURATION=10 \
SAT_RUNS=1 \
./tests/benchmark/run_benchmark.sh 8080

# Skip saturation phase to only run scenario tests:
SKIP_SATURATION=1 ./tests/benchmark/run_benchmark.sh 8080
```

### Running Standalone Protocol Throughput

You can also run the raw protocol throughput binary directly:

```bash
./build/protocol_throughput_benchmark 8080
```

---

<div class="page-nav">
  <a href="./architecture-and-examples.html" class="page-nav-link">
    <span class="page-nav-subtitle">Previous Section</span>
    <span class="page-nav-title">&larr; Architecture & Examples</span>
  </a>
  <a href="./index.html" class="page-nav-link page-nav-next">
    <span class="page-nav-subtitle">Documentation Home</span>
    <span class="page-nav-title">Overview &rarr;</span>
  </a>
</div>
