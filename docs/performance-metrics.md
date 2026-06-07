---
layout: default
---

[< Previous: Advanced Usage](./advanced-usage.html) | [🏠 Home](./) | [Next: Architecture & Examples >](./architecture-and-examples.html)
<hr>

# Performance, Tuning, and SSL/TLS

`HttpServer` exposes its underlying `cpptcpnet::TcpListener` instance via the `GetListener()` method. This allows you to configure socket options, apply connection profiles, enable SSL/TLS, and track network performance metrics exactly as you would in a raw TCP application.

---

## 1. Accessing the Listener

To configure or query the underlying network layer, call `server.GetListener()`:

```cpp
cpphttp::HttpServer server(8080);

// Get a reference to the underlying TcpListener
cpptcpnet::TcpListener& listener = server.GetListener();
```

---

## 2. SSL/TLS Encryption

To secure HTTP traffic (HTTPS), configure the underlying TCP listener with SSL certificates before starting the server:

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

You can fine-tune socket-level settings to optimize HTTP performance for your network conditions:

```cpp
// Disable Nagle's algorithm (lower latency for small responses)
server.GetListener().SetNoDelay(true);

// Set kernel buffer sizes
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
You can also apply preset `ConnectionProfile` configurations:

```cpp
// Apply ReliableLAN profile for local high-speed routing
server.GetListener().SetDefaultConnectionProfile(
    cpptcpnet::ConnectionProfile::ReliableLAN()
);
```

### WebSocket Connection Profiles

You can apply dynamic connection profiles on the fly to individual sessions. When using WebSockets, you can configure unique keep-alive, idle timeout, and Nagle options via `WebSocketBehavior::connection_profile`, which are applied automatically upon upgrading:

```cpp
cpphttp::WebSocketBehavior ws_behavior;

// Override default WebSocket profile settings (defaults to interactive LAN)
ws_behavior.connection_profile.no_delay = true;
ws_behavior.connection_profile.idle_timeout = std::chrono::hours(2); // keep open for 2 hours

server.WebSocket("/ws", std::move(ws_behavior));
```

### Configuring Request, Timeout, and Redirect Limits

To protect servers and clients against memory-exhaustion and slow-rate DoS attacks, or to customize request timeouts and redirect behavior:

```cpp
// 1. Configure HttpServer
server.SetMaxHeaderSize(16384);          // Allow up to 16KB headers (default: 8KB)
server.SetMaxBodySize(50 * 1024 * 1024); // Reject bodies larger than 50MB (default: 10MB)
server.SetIdleTimeout(std::chrono::milliseconds(30000)); // Set idle worker timeout (default: 60s)

// 2. Configure HttpClient
cpphttp::HttpClient client("127.0.0.1", 8080);
client.SetTimeout(std::chrono::seconds(5)); // Timeout requests after 5 seconds
client.SetMaxBodySize(10 * 1024 * 1024);    // Reject response bodies larger than 10MB (default: 10MB)
client.SetMaxRedirects(10);                 // Set max redirects followed (default: 5)

// 3. Configure WebSocketClient
cpphttp::WebSocketClient ws_client;
ws_client.SetMaxBodySize(20 * 1024 * 1024); // Set max WebSocket frame size limit (default: 16MB)
ws_client.SetCloseTimeout(std::chrono::milliseconds(2000)); // Set WebSocket close handshake timeout (default: 1s)
```

### Thread Pool Tuning (HttpServer)

`HttpServer` relies on `cpp-tcpnet`'s internal worker pool to dispatch received data. By default, it uses `std::thread::hardware_concurrency()` worker threads. The worker pool uses **session-affinity hashing** \u2014 each connection is pinned to a specific thread (via `session_id % num_threads`), so multiple threads can run in parallel without corrupting per-connection state.

You can tune the server's worker thread count via the underlying listener:

```cpp
cpphttp::HttpServer server(8080);

// Use a fixed number of worker threads (default: hardware concurrency)
server.GetListener().SetWorkerThreadCount(4);

server.Start();
```

> **Note:** Because session-affinity is maintained, all thread count values ≥ 1 are safe. Higher counts increase parallelism across simultaneous connections; there is no need to set it to 1 for ordering correctness.

### Thread Pool Tuning (HttpClient)

By default, all instances of `HttpClient` share a global background `cppasyncworker::WorkerPool` for processing asynchronous requests (`GetAsync`, `PostAsync`, etc.). You can customize the size of this global worker pool or pass a custom pool to individual clients:

```cpp
// 1. Customize the size of the global background thread pool
cpphttp::SetClientWorkerPoolSize(8); // Spawn 8 worker threads for shared async HTTP tasks (defaults to hardware concurrency, min: 4)

// 2. Or, construct an HttpClient using a fully custom, independent worker pool
auto my_pool = std::make_shared<cppasyncworker::WorkerPool>(16);
cpphttp::HttpClient client("127.0.0.1", 8080, my_pool);
```

---

## 4. Throughput Tracking

You can monitor real-time HTTP upload and download rates (bytes/second) by passing the listener's event broker to the `ThroughputTracker` utility:

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
            std::cout << "Upload rate: " << tracker.GetSendThroughputBytesPerSec() << " B/s | "
                      << "Download rate: " << tracker.GetRecvThroughputBytesPerSec() << " B/s\n";
        }
    }).detach();

    std::cin.get();
    server.Stop();
}
```

---

## 5. Direct Stats (Atomic Counters)

At any time, you can query cumulative bytes/packets handled by the server:

```cpp
cpptcpnet::ListenerStats stats = server.GetListener().GetStats();

std::cout << "Active HTTP Connections: " << stats.active_connections << "\n"
          << "Total Bytes Transmitted: " << stats.bytes_sent << " bytes\n"
          << "Total Bytes Received:    " << stats.bytes_received << " bytes\n";
```

---
[< Previous: Advanced Usage](./advanced-usage.html) | [🏠 Home](./) | [Next: Architecture & Examples >](./architecture-and-examples.html)
