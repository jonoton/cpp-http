---
layout: default
---

[< Previous: Performance & Tuning](./performance-metrics.html) | [🏠 Home](./)
<hr>

# Architecture & Threading Model

`cpp-http` inherits its multi-threaded, asynchronous architecture from `cpp-tcpnet`. This ensures that network IO, request parsing, and route callback processing do not block your main application logic.

<div class="mermaid">
graph TD
    classDef client fill:#fdf4ff,stroke:#d946ef,stroke-width:2px;
    classDef listener fill:#eff6ff,stroke:#3b82f6,stroke-width:2px;
    classDef worker fill:#ecfdf5,stroke:#10b981,stroke-width:2px;
    classDef handler fill:#fff1f2,stroke:#f43f5e,stroke-width:2px;

    Client[Client Browser/API]:::client -->|1. HTTP Request| TcpListener[TcpListener Poll Thread]:::listener
    TcpListener -->|2. TCP Bytes| WorkerPool[Worker Pool Thread]:::worker
    subgraph WorkerPoolThread [Worker Pool Task]
        WorkerPool -->|3. HandleIncomingData| Reassembly[Rebuild & Buffer Fragments]:::worker
        Reassembly -->|4. Parse Request| Router[HTTP Route Dispatcher]:::worker
        Router -->|5. Execute Callback| Handler[User Callback Lambda]:::handler
    end
    Handler -->|6. HttpResponse| TcpListener
    TcpListener -->|7. TCP Send| Client
</div>

---

## The Three Threading Pillars

1. **The Poller Thread:**
   The underlying `TcpListener` spawns a background thread that monitors client sockets using non-blocking OS selectors (`poll` or `WSAPoll`). This thread is responsible for receiving TCP byte blocks and sending outgoing responses.

2. **The Worker Pool:**
   Whenever new TCP bytes are read, they are pushed as work items onto a `cppasyncworker::WorkerPool` thread pool. The `HandleIncomingData` protocol engine runs concurrently on these worker threads, parsing the HTTP headers and executing your route handler callbacks.

3. **The Event Cleanup Thread:**
   To clean up session buffers when connections drop, `HttpServer` subscribes to client disconnect events via `cpppubsub::PubSub`. It runs a dedicated, detached background thread that checks for disconnected session events and safely deletes inactive buffers from the `sessions_` map.

---

## Sibling Libraries

`cpp-http` acts as a protocol layer on top of a highly optimized stack:
- **[cpp-tcpnet](https://github.com/jonoton/cpp-tcpnet):** The foundational socket management, polling loops, and TLS engine.
- **[cpp-pubsub](https://github.com/jonoton/cpp-pubsub):** The event broker that coordinates state cleanups.
- **[cpp-asyncworker](https://github.com/jonoton/cpp-asyncworker):** The worker thread pool scheduling network tasks.

---
[< Previous: Performance & Tuning](./performance-metrics.html) | [🏠 Home](./)
