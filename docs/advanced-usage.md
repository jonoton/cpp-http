---
layout: default
---

[< Previous: Basic Usage](./basic-usage.html) | [🏠 Home](./) | [Next: Performance & Tuning >](./performance-metrics.html)
<hr>

# Advanced Usage

This section explores advanced HTTP request processing, handling JSON payloads, multi-threaded safety, and defragmentation concepts inside `cpp-http`.

## JSON Payloads

Because `cpp-http` stores raw body strings in `HttpRequest::body`, you can easily integrate any JSON library (like `nlohmann/json`) inside your route handlers:

```cpp
#include "cpphttp.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

int main() {
    cpphttp::HttpServer server(8080);

    server.Post("/api/users", [](const cpphttp::HttpRequest& request) {
        cpphttp::HttpResponse response;
        response.headers["Content-Type"] = "application/json";

        try {
            // Parse raw request body into JSON
            auto req_json = json::parse(request.body);
            std::string username = req_json.at("username");

            // Build success JSON response
            json res_json;
            res_json["status"] = "success";
            res_json["message"] = "User " + username + " registered successfully!";
            
            response.status_code = 200;
            response.body = res_json.dump();
        } catch (const std::exception& e) {
            // Build error JSON response
            json err_json;
            err_json["status"] = "error";
            err_json["error_message"] = e.what();

            response.status_code = 400;
            response.status_message = "Bad Request";
            response.body = err_json.dump();
        }

        return response;
    });

    server.Start();
    // Wait...
}
```

## Dynamic Routing & Path Parameters

`cpp-http` supports dynamic routing segments using the `:param` syntax and wildcard segments using the `*` suffix:

- **Path Parameters (`:param`)**: Captures a single path segment between slashes.
- **Wildcard Segments (`*`)**: Matches all remaining subpaths and constructs a single string.

```cpp
// 1. Path parameters matching (e.g. GET /users/123/profile)
server.Get("/users/:id/profile", [](const cpphttp::HttpRequest& request) {
    std::string id = request.path_params.at("id"); // captures "123"
    return cpphttp::HttpResponse::Plain("User profile: " + id);
});

// 2. Wildcard matching (e.g. GET /assets/js/jquery.js)
server.Get("/assets/*", [](const cpphttp::HttpRequest& request) {
    std::string filename = request.path_params.at("*"); // captures "js/jquery.js"
    return cpphttp::HttpResponse::Plain("File requested: " + filename);
});
```

## Query Parameters & URL Decoding

Query parameters (e.g., `?q=hello&sort=desc`) are automatically stripped from routes before path matching to ensure request dispatching runs correctly. They are decoded and stored inside `HttpRequest::query_params`:

```cpp
server.Get("/search", [](const cpphttp::HttpRequest& request) {
    std::string search_query = request.query_params.count("q") ? request.query_params.at("q") : "";
    std::string sort = request.query_params.count("sort") ? request.query_params.at("sort") : "asc";

    return cpphttp::HttpResponse::Plain("Query: " + search_query + " (sorted " + sort + ")");
});
```

All route parameters, path names, and query values are percent-decoded automatically (e.g., `%20` becomes a space character ` `).

## Middleware Pipeline

You can register middleware handlers that run sequentially before any request matches a route. If any middleware returns `false`, the request chain is aborted, and the middleware's response is returned immediately (useful for authentication or validation).

`cpp-http` supports both **global** and **path-scoped** middleware:

### 1. Global Middleware
```cpp
// Register global logging middleware (applies to all routes)
server.Use([](cpphttp::HttpRequest& request, cpphttp::HttpResponse& response) {
    std::cout << "[Middleware Log] Request on: " << request.path << "\n";
    return true;
});
```

### 2. Path-Scoped Middleware
```cpp
// Register scoped auth middleware (only runs on paths starting with "/api")
server.Use("/api", [](cpphttp::HttpRequest& request, cpphttp::HttpResponse& response) {
    if (request.GetHeader("Authorization") != "secret-token") {
        response = cpphttp::HttpResponse::Plain("Unauthorized", 401);
        return false; // abort request chain
    }
    return true; // continue request processing
});
```

### 3. Rate Limiting Middleware

`cpp-http` provides a built-in `RateLimiter` middleware class to limit requests per IP address over a sliding time window.

It extracts the client's IP from the `X-Forwarded-For` HTTP header (falling back to the TCP listener's peer address). If an IP exceeds the configured request limit in the given time window, the rate limiter aborts the middleware chain, returns a `429 Too Many Requests` status, and sets a `Retry-After` header indicating when the client can try again.

#### Global and Scoped Rate Limiting

Since `RateLimiter` is implemented as standard middleware, you can register it **globally** to protect the entire server, or **scoped** to apply stricter limit configurations for specific endpoints or path prefixes:

```cpp
#include "cpphttp.hpp"

int main() {
    cpphttp::HttpServer server(8080);

    // 1. Global rate limiting (applies to all routes)
    // Limits each client IP to at most 100 requests every 60 seconds
    server.Use(cpphttp::RateLimiter(100, std::chrono::seconds(60)));

    // 2. Scoped rate limiting (applies only to paths starting with "/api/login")
    // Stricter limits for login endpoints: max 5 attempts per minute
    server.Use("/api/login", cpphttp::RateLimiter(5, std::chrono::seconds(60)));

    server.Get("/api/resource", [](const cpphttp::HttpRequest& req) {
        return cpphttp::HttpResponse::Plain("Success!");
    });

    server.Start();
}
```

### 4. CORS Middleware

`cpp-http` provides a built-in configurable `Cors` middleware class to handle Cross-Origin Resource Sharing (CORS).

It automatically intercepts preflight `OPTIONS` requests and returns a `204 No Content` response with the appropriate `Access-Control-Allow-*` headers. For actual requests, it injects the necessary CORS headers into the final response.

```cpp
#include "cpphttp.hpp"

int main() {
    cpphttp::HttpServer server(8080);

    // Configure CORS settings
    cpphttp::CorsConfig cors_config;
    cors_config.allow_origin = "https://example.com";
    cors_config.allow_credentials = true;
    cors_config.expose_headers = {"X-Custom-Header"};
    
    // 1. Global CORS (applies to all routes)
    // server.Use(cpphttp::Cors(cors_config));

    // 2. Scoped CORS (applies only to paths starting with "/api")
    server.Use("/api", cpphttp::Cors(cors_config));

    server.Get("/api/data", [](const cpphttp::HttpRequest& req) {
        return cpphttp::HttpResponse::Plain("Data with CORS");
    });

    server.Start();
}
```

## TCP Defragmentation & Pipelining

In TCP connections, data packets can arrive fragmented (broken into multiple chunks) or coalesced (multiple requests sent sequentially in one TCP stream, known as HTTP pipelining). `cpp-http` handles both scenarios automatically:

1. **Fragmentation Protection:**
   The server buffers incoming bytes inside an internal session buffer. It searches for the header terminator (`\r\n\r\n`). If not found, it waits for more TCP packets. Once the header is found, it extracts `Content-Length`. If the buffered data is smaller than the body size, it defers routing and waits for more packet events.
   
2. **Pipelining Support:**
   After a route callback is executed and the response is sent, the server clears only the processed request block from the session buffer, leaving subsequent HTTP requests untouched in the queue:
   ```cpp
   buffer.erase(buffer.begin(), buffer.begin() + body_start_idx + content_length);
   ```

## Routing and 404 Handler

When a client sends a request, the server maps the HTTP method and path into a routing key: `Method:Path` (e.g., `GET:/api/users`). 

If no route exists in the routing map matching the incoming key, `cpp-http` automatically generates and returns a standard plain-text 404 response:

```cpp
HttpResponse not_found;
not_found.status_code = 404;
not_found.status_message = "Not Found";
not_found.headers["Content-Type"] = "text/plain";
not_found.body = "404 - Route not found.";
```

## Multi-Threaded Safety

`HttpServer` dispatches incoming data to a worker pool provided by `cpp-tcpnet`. The worker pool uses **session-affinity hashing** — all packets for a given TCP connection are always routed to the same worker thread. This guarantees in-order delivery per connection while still processing different connections in parallel across all available CPU cores.

Route callbacks may execute concurrently across connections, so:
- Route callbacks **must be thread-safe** with respect to any shared state.
- Protect access to global variables or shared resources using synchronization primitives (e.g., `std::mutex`, `std::atomic`):

```cpp
#include <mutex>

std::mutex db_mutex;
int user_count = 0;

server.Post("/register", [](const cpphttp::HttpRequest& req) {
    std::lock_guard<std::mutex> lock(db_mutex);
    user_count++;
    // safely perform modifications...
});
```

## HTTP Streaming

`cpp-http` supports streaming for both large **uploads** (server-side) and large **downloads** (client-side), allowing data to be processed incrementally without buffering the entire payload in memory.

### Server-Side: `AddStreamRoute`

Register a streaming upload route using `AddStreamRoute`. The handler is invoked **incrementally** for each received chunk. When `is_final` is `true`, the complete body has been received and you should return your final `HttpResponse`:

```cpp
#include "cpphttp.hpp"
#include <iostream>

int main() {
    cpphttp::HttpServer server(8080);

    // Accumulate the total bytes received across chunks
    server.AddStreamRoute("POST", "/upload",
        [](const cpphttp::HttpRequest& req,
           const std::string& chunk,
           bool is_final) -> std::optional<cpphttp::HttpResponse> {

            std::cout << "Received chunk of " << chunk.size() << " bytes\n";

            if (is_final) {
                // Return the response only when all data has arrived
                return cpphttp::HttpResponse::Plain("Upload complete!");
            }
            return std::nullopt; // Signal: keep streaming
        });

    server.Start();
    // ...
}
```

Key points:
- Supports both `Content-Length` and **Chunked Transfer Encoding** (`Transfer-Encoding: chunked`) upload bodies.
- The handler signature is `std::optional<HttpResponse>(const HttpRequest&, const std::string& chunk, bool is_final)`.
- Returning `std::nullopt` tells the server to continue accumulating; returning a value sends the response and closes the stream.
- Bodies exceeding `SetMaxBodySize` automatically return `413 Payload Too Large`.

### Client-Side: Streaming Downloads (`GetStream` / `PostStream`)

Use `GetStream` or `PostStream` to receive large responses incrementally instead of buffering the full body:

```cpp
cpphttp::HttpClient client("127.0.0.1", 8080);

// Stream a large file download chunk-by-chunk
cpphttp::HttpResponse res = client.GetStream(
    "/large-file",
    [](const cpphttp::HttpResponse& partial_res,
       const std::string& chunk,
       bool is_final) {
        std::cout << "Got " << chunk.size() << " bytes";
        if (is_final) std::cout << " (final)";
        std::cout << "\n";
    });

std::cout << "Status: " << res.status_code << "\n";
```

### Client-Side: Streaming Uploads (`PostStreamAsync` / `PutStreamAsync`)

For large uploads, use the async stream upload API. Provide a `stream_provider` callback that returns the next chunk of data to send, and a `content_length` for the `Content-Length` header (or use chunked encoding via headers):

```cpp
cpphttp::HttpClient client("127.0.0.1", 8080);

// Stream a 10MB upload in 1MB chunks
std::string large_data(10 * 1024 * 1024, 'X');
size_t offset = 0;
const size_t chunk_size = 1024 * 1024;

auto future = client.PostStreamAsync(
    "/upload",
    {/* headers */},
    [&](size_t /*hint*/) -> std::string {
        if (offset >= large_data.size()) return ""; // Signal EOF
        size_t take = std::min(chunk_size, large_data.size() - offset);
        std::string chunk = large_data.substr(offset, take);
        offset += take;
        return chunk;
    },
    large_data.size() // Content-Length
);

cpphttp::HttpResponse res = future.get();
std::cout << "Upload status: " << res.status_code << "\n";
```

The `PutStreamAsync` API is identical but sends an HTTP `PUT` request.

## Multipart Form-Data Parsing

`cpp-http` provides a `ParseMultipart` utility to handle file uploads and form-data. It splits the request body based on the boundary token extracted from the `Content-Type` header:

```cpp
#include "cpphttp.hpp"

server.Post("/upload", [](const cpphttp::HttpRequest& request) {
    // 1. Get multipart boundary from Content-Type header
    std::string ct = request.GetHeader("Content-Type");
    size_t boundary_pos = ct.find("boundary=");
    if (boundary_pos == std::string::npos) {
        return cpphttp::HttpResponse::Plain("Bad Request", 400);
    }
    std::string boundary = ct.substr(boundary_pos + 9);

    // 2. Parse request body
    std::vector<cpphttp::MultipartPart> parts = cpphttp::ParseMultipart(request.body, boundary);
    
    for (const auto& part : parts) {
        std::cout << "Part Name: " << part.name << "\n";
        if (!part.filename.empty()) {
            std::cout << "Uploaded File: " << part.filename << " (" << part.content_type << ")\n";
            // part.data contains raw file content
        }
    }

    return cpphttp::HttpResponse::Plain("Upload completed!");
});
```

## Serving Static Files

`cpp-http` provides a convenient built-in mechanism to serve an entire directory of static files using `HttpServer::StaticDir`:

```cpp
server.StaticDir(const std::string &route_prefix, const std::string &directory_path);
```

- **`route_prefix`**: The HTTP route prefix under which static files will be served (e.g., `/static` or `/assets`).
- **`directory_path`**: The local directory containing the static assets.

### Example Usage

```cpp
#include "cpphttp.hpp"
#include <iostream>

int main() {
    cpphttp::HttpServer server(8080);

    // Serve files in the "public" local directory under "/static"
    // For example:
    // - Local file: "public/index.html" -> URL: "http://localhost:8080/static/index.html"
    // - Local file: "public/css/style.css" -> URL: "http://localhost:8080/static/css/style.css"
    server.StaticDir("/static", "./public");

    server.Start();
    // Wait...
}
```

### Features & Security
- **Automatic MIME-type mapping:** The server automatically determines and sets the correct `Content-Type` header based on the file extension (supporting common formats like `.html`, `.css`, `.js`, `.json`, `.png`, `.jpg`/`.jpeg`, `.gif`, `.svg`, `.txt`, `.ico`, `.xml`, and `.pdf`).
- **Path Traversal Protection:** The routing engine automatically sanitizes `.` and `..` path segments, ensuring clients cannot read files outside of the registered directory.
- **Directory Access Prevention:** Requests attempting to load directories rather than files will automatically return a `404 Not Found` response.
- **Compression & HEAD Support:** Static files automatically benefit from standard features like transparent Gzip compression and automatic `HEAD` method fallback.

## Response Caching

To optimize server performance, `cpp-http` provides a `ResponseCache` utility to cache handler responses. It is implemented as a wrapper around route handlers, which intercepts request processing to serve cached responses on hits, and caches new responses on misses.

### Key Characteristics:
- **Scope**: Caches only `GET` requests returning a `200 OK` status.
- **Exclusions**: Skips file-based responses (`is_file == true`) to prevent loading large static files into RAM.
- **Cache Key**: Generated using the request path combined with normalized (sorted) query parameters to ensure consistency.
- **Lifetime**: Uses `std::shared_ptr` internals, meaning cached state is safely preserved across asynchronous worker threads.

### Example Usage:

```cpp
#include "cpphttp.hpp"

int main() {
    cpphttp::HttpServer server(8080);
    
    // Create a response cache with a default 10-second TTL
    cpphttp::ResponseCache cache(std::chrono::seconds(10));

    // Wrap a route handler to cache its response for the default duration
    server.Get("/time", cache.Wrap([](const cpphttp::HttpRequest& req) {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        return cpphttp::HttpResponse::Plain("Current time: " + std::string(std::ctime(&now)));
    }));

    // Wrap with a custom cache duration (e.g. 5 seconds)
    server.Get("/fast-expire", cache.Wrap([](const cpphttp::HttpRequest& req) {
        return cpphttp::HttpResponse::Plain("This cache expires fast!");
    }, std::chrono::seconds(5)));

    server.Start();
}
```

## Transparent HTTP Compression (Gzip)

To optimize network bandwidth, `cpp-http` supports automatic Gzip compression and decompression out-of-the-box (powered by `zlib`):

- **Server-Side:** If the client sends an `Accept-Encoding: gzip` header, the `HttpServer` automatically compresses responses transparently before sending them back.
- **Client-Side:** The `HttpClient` sends `Accept-Encoding: gzip` by default. If the server replies with `Content-Encoding: gzip`, `HttpClient` automatically decompresses the body before returning the `HttpResponse` object.

## HTTP Redirect Method Downgrade

When the client performs redirect following (e.g. following a `302 Found` or `303 See Other` response status), it automatically downgrades `POST` requests to `GET` requests for the redirected URL. This aligns with standard web-browser redirection behavior.

## WebSocket Support

`cpp-http` provides comprehensive support for WebSockets, allowing you to build both WebSocket servers and clients using a clean, asynchronous event-driven API.

### 1. Server-Side WebSocket Routing

To handle WebSocket upgrades on the server, define a `WebSocketBehavior` struct specifying event callbacks, then register it using `server.WebSocket(...)`:

```cpp
#include "cpphttp.hpp"
#include <iostream>

int main() {
    cpphttp::HttpServer server(8080);

    cpphttp::WebSocketBehavior ws_behavior;
    
    // Callback when a client completes the handshake and opens the connection
    ws_behavior.on_open = [](std::shared_ptr<cpphttp::WebSocketConnection> conn) {
        std::cout << "Client connected: session_id=" << conn->GetSessionId() << "\n";
    };

    // Callback when a text message frame is received
    ws_behavior.on_message = [](std::shared_ptr<cpphttp::WebSocketConnection> conn, const std::string &msg) {
        std::cout << "Received text: " << msg << "\n";
        conn->Send("Echo: " + msg); // Echo the text message back to the client
    };

    // Callback when a binary message frame is received
    ws_behavior.on_binary = [](std::shared_ptr<cpphttp::WebSocketConnection> conn, const std::vector<uint8_t> &data) {
        std::cout << "Received binary frame of size: " << data.size() << " bytes\n";
        conn->SendBinary(data); // Echo binary data back
    };

    // Callback when connection closes
    ws_behavior.on_close = [](std::shared_ptr<cpphttp::WebSocketConnection> conn) {
        std::cout << "Client disconnected\n";
    };

    // Register WebSocket route
    server.WebSocket("/ws", std::move(ws_behavior));

    server.Start();
    // Wait...
}
```

The server-side connection object (`WebSocketConnection`) provides helper methods to interact with the socket:
* `bool Send(const std::string &message)`: Send a text frame.
* `bool SendBinary(const std::vector<uint8_t> &data)`: Send a binary frame.
* `bool Ping(const std::string &payload)`: Send a ping frame.
* `void Close(uint16_t status_code, const std::string &reason)`: Send a close frame and shut down the connection.

### 2. Client-Side WebSocket (`WebSocketClient`)

The library includes a dedicated client-side `WebSocketClient` class to connect to any WebSocket server (supporting both `ws://` and `wss://` protocols):

```cpp
#include "cpphttp.hpp"
#include <iostream>
#include <thread>

int main() {
    cpphttp::WebSocketClient client;

    // Register event callbacks
    client.OnOpen([]() {
        std::cout << "Connected to server!\n";
    });

    client.OnMessage([](const std::string &msg) {
        std::cout << "Received message: " << msg << "\n";
    });

    client.OnBinary([](const std::vector<uint8_t> &data) {
        std::cout << "Received binary data of size: " << data.size() << " bytes\n";
    });

    client.OnClose([](uint16_t status_code, const std::string &reason) {
        std::cout << "Closed connection: code=" << status_code << ", reason=" << reason << "\n";
    });

    // Connect asynchronously to WebSocket endpoint
    if (client.Connect("ws://127.0.0.1:8080/ws")) {
        client.Send("Hello from WebSocketClient!");
        
        // Sleep to allow background worker to process responses
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        client.Close();
    } else {
        std::cerr << "Failed to connect to server\n";
    }

    return 0;
}
```

The client-side `WebSocketClient` exposes:
* `void OnOpen(WsConnectHandler handler)`: Sets callback for when connection opens.
* `void OnMessage(WsMessageHandler handler)`: Sets callback for incoming text messages.
* `void OnBinary(WsBinaryHandler handler)`: Sets callback for incoming binary data.
* `void OnClose(WsCloseHandler handler)`: Sets callback for when connection closes.
* `bool Connect(const std::string &url, std::chrono::milliseconds timeout)`: Initiates connection and performs handshake.
* `bool Send(const std::string &message)`: Sends a text message.
* `bool SendBinary(const std::vector<uint8_t> &data)`: Sends binary data.
* `bool Ping(const std::string &payload)`: Sends a ping frame.
* `void Close(uint16_t status_code, const std::string &reason)`: Closes the connection cleanly.
* `void Disconnect()`: Immediately terminates connection.

---
[< Previous: Basic Usage](./basic-usage.html) | [🏠 Home](./) | [Next: Performance & Tuning >](./performance-metrics.html)
