---
layout: default
title: Advanced Usage
description: Dynamic routing, middleware pipelines, CORS, rate limiting, caching, streaming, compression, WebSockets, and HTTP/3 QUIC.
---

# Advanced Usage

This guide covers advanced features in `cpp-http`, including dynamic routing, middleware, security controls, streaming, compression, WebSockets, TLS, and HTTP/3 over QUIC.

## Table of Contents

- [Advanced Request Handling (JSON & Multipart)](#advanced-request-handling-json--multipart)
- [Middleware Pipeline](#middleware-pipeline)
- [CORS Middleware](#cors-middleware)
- [Fixed-Window Rate Limiting](#fixed-window-rate-limiting)
- [Response Caching](#response-caching)
- [Transparent HTTP Compression](#transparent-http-compression)
- [HTTP Streaming](#http-streaming)
- [Static Files and SPA Mode](#static-files-and-spa-mode)
- [WebSocket Support](#websocket-support)
- [TLS and Redirect Scoping](#tls-and-redirect-scoping)
- [HTTP/3 and Hybrid Transports](#http3-and-hybrid-transports)
- [Limits and DoS Protection](#limits-and-dos-protection)
- [Multi-Threaded Safety and Logging](#multi-threaded-safety-and-logging)
- [TCP Defragmentation and Pipelining](#tcp-defragmentation-and-pipelining)

---

## Advanced Request Handling (JSON & Multipart)

For standard route definitions, path parameters (`:param`), query string extraction, and multi-headers, refer to the [Basic Usage Guide](./basic-usage.html).

`cpp-http` uses an `O(1)` exact-match hash table (`METHOD:path`) for fixed endpoints. If an exact match is not found, the server evaluates pattern routes (`:param` and `*`) in registration order.

### JSON Payloads

Because `HttpRequest::body` holds the raw payload string, you can parse and serialize JSON with any JSON library (such as `nlohmann/json`) inside your route handlers:

```cpp
#include "cpphttp.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

server.Post("/api/users", [](const cpphttp::HttpRequest& request) {
    try {
        auto req_json = json::parse(request.body);
        std::string username = req_json.at("username");

        json res_json;
        res_json["status"] = "success";
        res_json["message"] = "User " + username + " registered!";
        return cpphttp::HttpResponse::Json(res_json.dump(), 200);
    } catch (const std::exception& e) {
        json err_json;
        err_json["status"] = "error";
        err_json["error_message"] = e.what();
        return cpphttp::HttpResponse::Json(err_json.dump(), 400);
    }
});
```

### Multipart Form-Data Parsing

Use `cpphttp::ParseMultipart(body, boundary)` to parse `multipart/form-data` form fields and uploaded files into `std::vector<cpphttp::MultipartPart>`:

```cpp
server.Post("/upload", [](const cpphttp::HttpRequest& request) {
    std::string ct = request.GetHeader("Content-Type");
    size_t boundary_pos = ct.find("boundary=");
    if (boundary_pos == std::string::npos) {
        return cpphttp::HttpResponse::Plain("Bad Request", 400);
    }
    std::string boundary = ct.substr(boundary_pos + 9);

    auto parts = cpphttp::ParseMultipart(request.body, boundary);
    for (const auto& part : parts) {
        std::cout << "Part: " << part.name;
        if (!part.filename.empty()) {
            std::cout << " (file=" << part.filename
                      << ", type=" << part.content_type << ")";
        }
        std::cout << " size=" << part.data.size() << "\n";
    }
    return cpphttp::HttpResponse::Plain("Upload completed!");
});
```

---

## Middleware Pipeline

Middleware callbacks run sequentially before route matching or WebSocket upgrades. Each middleware has signature `bool(HttpRequest& request, HttpResponse& response)`. Returning `true` continues the pipeline; returning `false` short-circuits immediately and sends `response` to the client.

When middlewares return `true`, any headers or `multi_headers` they added to `response` are automatically merged into the final route or WebSocket upgrade response (without overwriting headers explicitly set by the route handler).

### Global and Path-Scoped Middleware

```cpp
// 1. Global middleware (runs on every dispatched request)
server.Use([](cpphttp::HttpRequest& request, cpphttp::HttpResponse& response) {
    response.headers["X-Frame-Options"] = "DENY";
    return true;
});

// 2. Path-scoped middleware (matches paths starting with "/api")
server.Use("/api", [](cpphttp::HttpRequest& request,
                      cpphttp::HttpResponse& response) {
    if (request.GetHeader("Authorization") != "secret-token") {
        response = cpphttp::HttpResponse::Plain("Unauthorized", 401);
        return false; // Abort pipeline
    }
    return true;
});
```

> **Note:** Path-scoped middleware uses a prefix check (`req.path.rfind(path_prefix, 0) == 0`). For example, `"/api"` matches `"/api/users"` and `"/apiv2"`; use `"/api/"` if you want to restrict strictly to the `/api/` subtree.

> **Warning:** Streaming upload routes registered via `PostStream` or `PutStream` are dispatched incrementally as headers arrive and **bypass** the `middlewares_` pipeline. Validate authentication/headers directly inside your `StreamHandler` when `chunk` arrives or on the first invocation.

---

## CORS Middleware

`cpphttp::Cors` is a built-in middleware for Cross-Origin Resource Sharing. It injects `Access-Control-Allow-*` and `Access-Control-Expose-Headers` headers, and automatically intercepts `OPTIONS` preflight requests with a `204 No Content` response.

### Minimal Example

```cpp
cpphttp::HttpServer server(8080);

cpphttp::CorsConfig cors_config;
cors_config.allow_origin = "https://example.com";
cors_config.allow_credentials = true;
cors_config.expose_headers = {"X-Custom-Header"};

// Apply globally or scoped to a prefix
server.Use("/api", cpphttp::Cors(cors_config));

server.Get("/api/data", [](const cpphttp::HttpRequest& req) {
    return cpphttp::HttpResponse::Plain("Data with CORS");
});
```

### `CorsConfig` Options

| Field | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `allow_origin` | `std::string` | `"*"` | Value for `Access-Control-Allow-Origin` |
| `allow_methods` | `std::vector<std::string>` | *See below* | Joined into `Access-Control-Allow-Methods` on `OPTIONS` preflight |
| `allow_headers` | `std::vector<std::string>` | `{"*"}` | Joined into `Access-Control-Allow-Headers` on `OPTIONS` preflight |
| `expose_headers` | `std::vector<std::string>` | `{}` | Joined into `Access-Control-Expose-Headers` when non-empty |
| `allow_credentials` | `bool` | `false` | Sets `Access-Control-Allow-Credentials: true` when enabled |
| `max_age` | `std::chrono::seconds` | `86400s` | Sets `Access-Control-Max-Age` (24h) on `OPTIONS` preflight when `> 0` |

Default `allow_methods`:
```cpp
{"GET", "POST", "PUT", "DELETE", "OPTIONS", "PATCH", "HEAD"}
```

> **Warning:** Browsers reject `Access-Control-Allow-Credentials: true` when `Access-Control-Allow-Origin` is `"*"`. Whenever you set `allow_credentials = true`, set `allow_origin` to an explicit origin (such as `"https://app.example.com"`).

---

## Fixed-Window Rate Limiting

`cpphttp::RateLimiter` is a thread-safe middleware that limits requests per client IP (`HttpRequest::client_ip`) over a **fixed time window** starting at the client's first request in that window. When a client exceeds `max_requests` within `window_duration`, `RateLimiter` aborts the pipeline with `429 Too Many Requests` and sets `Retry-After` to `window_duration.count()`.

### Minimal Example

```cpp
cpphttp::HttpServer server(8080);

// Global limit: 100 requests per 60 seconds per IP
server.Use(cpphttp::RateLimiter(100, std::chrono::seconds(60)));

// Stricter scoped limit on login endpoints: 5 requests per 60 seconds per IP
server.Use("/api/login", cpphttp::RateLimiter(5, std::chrono::seconds(60)));
```

### Proxy Headers (`SetTrustProxyHeaders`)

By default, `HttpRequest::client_ip` is the underlying socket peer IP (`transport_->GetPeerAddress(session_id).ip`). If your server sits behind a trusted reverse proxy that sets `X-Forwarded-For`, enable proxy header resolution so `client_ip` uses the first address in `X-Forwarded-For`:

```cpp
server.SetTrustProxyHeaders(true); // Default: false
```

| Method | Default | Behavior |
| :--- | :--- | :--- |
| `SetTrustProxyHeaders(bool trust)` | `false` | When `true`, extracts the first comma-separated IP from `X-Forwarded-For`; falls back to peer IP if absent |
| `RateLimiter(size_t max_requests, std::chrono::seconds window_duration)` | — | Shared-state (`std::shared_ptr`) middleware safe across all worker threads |

> **Warning:** Never call `SetTrustProxyHeaders(true)` when clients can reach `cpp-http` directly. Any client could spoof `X-Forwarded-For` to bypass `RateLimiter` completely.

---

## Response Caching

`cpphttp::ResponseCache` wraps individual `RouteHandler` callbacks to cache `GET` `200 OK` responses in memory with a configurable TTL and bounded capacity. Because `ResponseCache` shares state via `std::shared_ptr`, it is safe across all worker threads.

### Minimal Example

```cpp
cpphttp::HttpServer server(8080);

// Default TTL: 10 seconds, max_entries: 1024 (default)
cpphttp::ResponseCache cache(std::chrono::seconds(10));

server.Get("/time", cache.Wrap([](const cpphttp::HttpRequest& req) {
    auto now = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    return cpphttp::HttpResponse::Plain(
        "Current time: " + std::string(std::ctime(&now)));
}));

// Override TTL per route (5 seconds)
server.Get("/fast-expire", cache.Wrap([](const cpphttp::HttpRequest& req) {
    return cpphttp::HttpResponse::Plain("Short-lived cached response");
}, std::chrono::seconds(5)));
```

### Configuration and Caching Rules

| Property | Behavior |
| :--- | :--- |
| Constructor | `ResponseCache(std::chrono::seconds default_duration, size_t max_entries = 1024)` (0 is clamped to 1) |
| Cache Key | Percent-escaped `req.path` + sorted `req.query_params` (`?k1=v1&k2=v2`) |
| Request Bypass | Non-`GET` requests, or requests with `Authorization` or `Cookie` headers bypass the cache |
| Storage Exclusions | Only `status_code == 200` and `!is_file` responses are stored; responses with `Set-Cookie` (in `headers` or `multi_headers`) or `Cache-Control: no-store` / `private` are never stored |
| Eviction | Expired entries are lazily pruned on lookup; when `size >= max_entries`, oldest entries are evicted first |

> **Tip:** `ResponseCache` stores uncompressed `HttpResponse` objects before `HttpServerImpl` performs per-request `Accept-Encoding` negotiation, so different clients requesting `zstd`, `br`, `gzip`, or `identity` all benefit from the same cached handler result.

---

## Transparent HTTP Compression

`cpp-http` includes built-in transparent compression and decompression for **Zstandard (`zstd`)**, **Brotli (`br`)**, and **Gzip (`gzip`)**.

- **Server-side negotiation:** Inspects `Accept-Encoding` (including `q=` weights and `*`) and prefers `zstd` > `br` > `gzip` > `identity`.
- **Client-side advertisement & decompression:** `HttpClient` sends `Accept-Encoding: zstd, br, gzip` by default and transparently decompresses responses matching `Content-Encoding`.
- **Static & file responses:** Responses with `is_file == true` and `status_code == 200` are compressed on the fly in chunks via `StreamCompressor` using `Transfer-Encoding: chunked` (when `request.version != "HTTP/1.0"`).

### Compression Options

| Method | Default | Valid Range | Notes |
| :--- | :--- | :--- | :--- |
| `SetZstdCompressionLevel(int level)` | `3` | `[ZSTD_minCLevel(), ZSTD_maxCLevel()]` | Throws `std::invalid_argument` if out of range |
| `SetBrotliQuality(int quality)` | `4` | `[0, 11]` | Throws `std::invalid_argument` if out of range |
| `SetGzipCompressionLevel(int level)` | `-1` (`Z_DEFAULT_COMPRESSION`) | `[-1, 9]` | Throws `std::invalid_argument` if out of range |
| `SetMinCompressionSize(size_t size)` | `1024` bytes | `>= 0` | Payloads/files smaller than this threshold are sent uncompressed |

```cpp
cpphttp::HttpServer server(8080);
server.SetZstdCompressionLevel(5);
server.SetBrotliQuality(5);
server.SetGzipCompressionLevel(1); // Fast gzip
server.SetMinCompressionSize(512);
```

### MIME Filtering and `Vary` Header

By default, `is_compressible_mimetype` compresses responses (including when `Content-Type` is empty) except known pre-compressed or binary container types:

- **Excluded images:** `image/jpeg`, `image/jpg`, `image/png`, `image/gif`, `image/webp`, `image/heic`, `image/tiff`, `image/x-icon`, `image/bmp` (`image/svg+xml` *is* compressed).
- **Excluded media:** all `video/*` and `audio/*` MIME types.
- **Excluded archives & binary types:** `application/zip`, `application/gzip`, `application/x-gzip`, `application/x-bzip2`, `application/x-tar`, `application/x-rar-compressed`, `application/x-7z-compressed`, `application/x-compress`, `application/pdf`, `application/octet-stream`.

Responses that already define `Content-Encoding` are left untouched. Eligible responses automatically receive `Vary: Accept-Encoding` (or have `Accept-Encoding` appended to an existing `Vary` header if not already present or `*`).

> **Note:** On the client, `zstd_decompress`, `brotli_decompress`, and `gzip_decompress` enforce `HttpClient::GetMaxBodySize()` (default 10 MB) on the **decompressed** output size to protect against compression bombs, throwing `std::runtime_error` if exceeded.

> **Warning:** `206 Partial Content` range responses and `HTTP/1.0` file transfers are never compressed by the server.

---

## HTTP Streaming

Use streaming APIs when handling large uploads or downloads so payloads do not have to be buffered completely in RAM.

### Server-Side Streaming Uploads (`PostStream`, `PutStream`)

Register streaming upload endpoints using `server.PostStream` or `server.PutStream` (the lower-level `AddStreamRoute` helper is private to `HttpServerImpl`). The `StreamHandler` callback signature is:

```cpp
using StreamHandler = std::function<std::optional<HttpResponse>(
    const HttpRequest& req, const std::string& chunk, bool is_final)>;
```

Return `std::nullopt` to keep receiving chunks, and return an `HttpResponse` once `is_final == true` (or early to reject/respond).

```cpp
server.PostStream("/upload",
    [](const cpphttp::HttpRequest& req,
       const std::string& chunk,
       bool is_final) -> std::optional<cpphttp::HttpResponse> {
        if (!chunk.empty()) {
            // Write chunk incrementally to disk or processor...
        }
        if (is_final) {
            return cpphttp::HttpResponse::Plain("Upload complete!");
        }
        return std::nullopt;
    });
```

> **Warning:**
> 1. Returning an `HttpResponse` before the full request body has arrived (`is_final == false`) triggers an early response: the server forces `Connection: close`, discards remaining wire bytes, and closes the connection to prevent HTTP request smuggling.
> 2. Returning `std::nullopt` when `is_final == true` produces `500 Internal Server Error`.
> 3. For `Transfer-Encoding: chunked` uploads on stream routes, chunks exceeding `SetMaxBodySize` immediately return `413 Payload Too Large` and close the connection.

### Client-Side Streaming Downloads (`GetStream` and `PostStream`)

Use `GetStream` or `PostStream` to process response body chunks as they arrive:

```cpp
cpphttp::HttpClient client("127.0.0.1", 8080);

cpphttp::HttpResponse res = client.GetStream(
    "/large-file",
    [](const cpphttp::HttpResponse& partial_res,
       const std::string& chunk,
       bool is_final) {
        if (!chunk.empty()) {
            // Consume chunk (partial_res holds status_code and headers)
        }
        if (is_final) {
            std::cout << "Download complete (status "
                      << partial_res.status_code << ")\n";
        }
    });
```

> **Note:** In streaming download mode (`on_chunk` provided), `HttpClient` skips the `Content-Length > max_body_size_` upfront rejection so arbitrarily large files can be streamed, and delivers raw body chunks without buffering the full body in `res.body`.

### Client-Side Streaming Uploads (`PostStreamAsync` and `PutStreamAsync`)

Supply a `stream_provider` callback `std::string(size_t ask_size)` and either an explicit `content_length` or `0` (or `Transfer-Encoding: chunked` in headers) for chunked transfer encoding. Return an empty string `""` from `stream_provider` to signal EOF.

{% raw %}
```cpp
cpphttp::HttpClient client("127.0.0.1", 8080);

std::string large_data(10 * 1024 * 1024, 'X');
size_t offset = 0;

// Fixed Content-Length upload (pass 0 as content_length for chunked encoding)
auto future = client.PostStreamAsync(
    "/upload",
    {{"Content-Type", "application/octet-stream"}},
    [&](size_t ask_size) -> std::string {
        if (offset >= large_data.size()) return "";
        size_t take = std::min(ask_size, large_data.size() - offset);
        std::string chunk = large_data.substr(offset, take);
        offset += take;
        return chunk;
    },
    large_data.size());

cpphttp::HttpResponse res = future.get();
```
{% endraw %}

`HttpClient` automatically throttles outbound streaming chunks using `GetTransferSubscriber()` so in-flight bytes stay bounded by `max_outbound_buffer_size / 2` (minimum `65536` bytes).

---

## Static Files and SPA Mode

`HttpServerImpl::StaticDir` serves a local directory asynchronously from disk without loading entire files into memory.

```cpp
void StaticDir(const std::string& route_prefix,
               const std::string& directory_path,
               bool spa_mode = false);
```

```cpp
cpphttp::HttpServer server(8080);

// Serve ./public under /static
server.StaticDir("/static", "./public");

// Serve a Single Page Application under /app with fallback to ./dist/index.html
server.StaticDir("/app", "./dist", true);
```

### Capabilities and Security

- **MIME-type mapping (`GetMimeType`):** Automatically sets `Content-Type` for 50+ extensions (`.html`, `.css`, `.js`/`.mjs`, `.json`, `.map`, `.svg`, `.png`, `.jpg`/`.jpeg`, `.webp`, `.gif`, `.ico`, `.wasm`, `.woff`/`.woff2`/`.ttf`/`.otf`, `.pdf`, `.mp4`, `.webm`, `.mp3`, `.wav`, `.zip`, `.gz`, `.yaml`, `.md`, etc.), defaulting unknown extensions to `application/octet-stream`.
- **Path traversal protection:** Canonicalizes both the root directory and target file path with `std::filesystem::weakly_canonical` and verifies the target stays strictly within the root directory.
- **Directory index:** Requests resolving to a directory automatically serve `<dir>/index.html` if present.
- **SPA fallback (`spa_mode = true`):** Missing files or traversal rejections fall back to `<directory_path>/index.html` (`200 OK`) so client-side routers can handle virtual URLs.
- **Byte-range requests:** Sets `Accept-Ranges: bytes` and handles `Range: bytes=START-END`, `bytes=START-`, and `bytes=-SUFFIX` with `206 Partial Content` and `Content-Range` (or `416 Range Not Satisfiable` when out of bounds).
- **Async chunked pumping & compression:** `HttpServerImpl::PumpFileTransfer` streams file chunks bounded by `ConnectionProfile::max_outbound_buffer_size / 2` and `send_chunk_size`, with on-the-fly compression for eligible `200 OK` files.

> **Tip:** To return a custom file manually from a regular route handler with the same zero-memory-bloat streaming and compression pipeline, populate `res.is_file = true`, `res.file_path`, `res.headers["Content-Type"]`, and `res.headers["Content-Length"]` (plus optional `res.file_offset` / `res.file_length` for partial transfers).

---

## WebSocket Support

`cpp-http` implements RFC 6455 WebSocket upgrades, frame masking/unmasking, message fragmentation/reassembly, and `Ping`/`Pong`/`Close` control frames on both server and client.

### Server-Side WebSockets

Register WebSocket endpoints with `server.WebSocket(path, behavior)`. Route patterns with `:param` and `*` are supported on WebSocket paths too.

```cpp
cpphttp::HttpServer server(8080);

cpphttp::WebSocketBehavior ws_behavior;
ws_behavior.on_open = [](std::shared_ptr<cpphttp::WebSocketConnection> conn) {
    std::cout << "Opened session " << conn->GetSessionId() << "\n";
};
ws_behavior.on_message = [](std::shared_ptr<cpphttp::WebSocketConnection> conn,
                            const std::string& msg) {
    conn->Send("Echo: " + msg);
};
ws_behavior.on_binary = [](std::shared_ptr<cpphttp::WebSocketConnection> conn,
                           const std::vector<uint8_t>& data) {
    conn->SendBinary(data);
};
ws_behavior.on_close = [](std::shared_ptr<cpphttp::WebSocketConnection> conn) {
    std::cout << "Closed session " << conn->GetSessionId() << "\n";
};

// Optional: customize TCP profile applied upon upgrade
ws_behavior.connection_profile.idle_timeout = std::chrono::hours(2);

server.WebSocket("/ws", std::move(ws_behavior));
```

`WebSocketConnection` methods:

| Method | Description |
| :--- | :--- |
| `uint64_t GetSessionId() const` | Returns the connection's session ID |
| `bool Send(const std::string& message)` | Sends a text frame (`opcode 0x01`) |
| `bool SendBinary(const std::vector<uint8_t>& data)` | Sends a binary frame (`opcode 0x02`) |
| `bool Ping(const std::string& payload = "")` | Sends a ping frame (`opcode 0x09`) |
| `void Close(uint16_t status_code = 1000, const std::string& reason = "")` | Sends a close frame (`opcode 0x08`) |

Default `WebSocketBehavior::connection_profile` sets `no_delay = true`, `keepalive_enabled = true` (`idle = 60s`, `interval = 15s`, `count = 4`), and `idle_timeout = 3600000ms` (1 hour).

### Client-Side WebSockets (`WebSocketClient`)

`WebSocketClient` connects to `ws://` and `wss://` URLs:

```cpp
cpphttp::WebSocketClient client;

client.OnOpen([]() { std::cout << "Connected!\n"; });
client.OnMessage([](const std::string& msg) {
    std::cout << "Text: " << msg << "\n";
});
client.OnBinary([](const std::vector<uint8_t>& data) {
    std::cout << "Binary: " << data.size() << "\n";
});
client.OnClose([](uint16_t code, const std::string& reason) {
    std::cout << "Closed (" << code << "): " << reason << "\n";
});

if (client.Connect("ws://127.0.0.1:8080/ws")) { // default timeout: 5s
    client.Send("Hello!");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    client.Close(); // default status_code = 1000, reason = ""
}
```

| Method | Default / Signature | Description |
| :--- | :--- | :--- |
| `Connect(url, timeout)` | `timeout = std::chrono::seconds(5)` | Parses `ws://`/`wss://`, opens TCP/TLS socket, validates `101` + `Sec-WebSocket-Accept` |
| `Send(message)` / `SendBinary(data)` | — | Sends masked text or binary frame |
| `Ping(payload)` | `payload = ""` | Sends masked ping frame |
| `Close(status_code, reason)` | `status_code = 1000, reason = ""` | Sends close frame and waits up to `close_timeout` before disconnecting |
| `Disconnect()` | — | Immediately closes socket and stops internal worker |
| `SetTlsConfig(config)` / `SetAllowSelfSigned(allow)` | `verify_peer = true` | Configures TLS verification for `wss://` connections |
| `SetMaxBodySize(size)` | `10 * 1024 * 1024` (10 MB) | Max incoming/reassembled WebSocket message size |
| `SetMaxOutboundBufferSize(size)` | `10 * 1024 * 1024` (10 MB) | Max outbound send queue size on `cpptcpnet::TcpClient` |
| `SetCloseTimeout(timeout)` | `std::chrono::milliseconds(1000)` | Wait duration for close handshake completion |

> **Note:** Calling `Disconnect()` or `Close()` from inside a `WebSocketClient` callback (`OnMessage`, `OnClose`, etc.) is safe: `internal::CallbackScope` detects in-callback teardown and defers thread joining to avoid self-deadlock.

---

## TLS and Redirect Scoping

### Server-Side TLS (`SslConfig` and QUIC Certificates)

For `HttpServer` (TCP), configure `cpptcpnet::TcpListener::SslConfig` before calling `server.Start()`:

```cpp
cpptcpnet::TcpListener::SslConfig ssl_config;
ssl_config.cert_file = "certs/server.crt";
ssl_config.key_file = "certs/server.key";
// Optional mutual TLS (mTLS):
// ssl_config.ca_file = "certs/ca.crt";
// ssl_config.require_client_cert = true;

server.GetListener().EnableSSL(ssl_config);
server.Start();
```

| `SslConfig` Field | Default | Description |
| :--- | :--- | :--- |
| `cert_file` / `key_file` | `""` | Server PEM certificate and private key paths |
| `ca_file` | `""` | CA certificate file for verifying client certificates |
| `require_client_cert` | `false` | Require and verify client certificates (mTLS) |
| `verify_depth` | `-1` | Certificate chain verification depth (`-1` = OpenSSL default) |
| `cipher_list` / `cipher_suites` | `""` | TLS 1.2 cipher list / TLS 1.3 cipher suites |
| `min_tls_version` / `max_tls_version` | `-1` | Minimum / maximum TLS protocol version (`-1` = library default) |

For `QuicHttpServer`, call `server.GetTransport()->GetServer().SetCertificate(cert_file, key_file)`. For `HybridHttpServer`, configure both transports (see [HTTP/3 and Hybrid Transports](#http3-and-hybrid-transports)).

### Client-Side TLS (`ClientTlsConfig`)

`HttpClient`, `HttpClientImpl<QuicClientTransport>`, and `WebSocketClient` share `cpphttp::ClientTlsConfig`. **Peer certificate and hostname verification are enabled by default (`verify_peer = true`).**

| `ClientTlsConfig` Field | Default | Description |
| :--- | :--- | :--- |
| `verify_peer` | `true` | Verify server certificate chain and hostname/IP SAN |
| `ca_file` | `""` | Path to trusted PEM CA file or self-signed server cert |
| `ca_path` | `""` | Path to directory of hashed CA certificates |
| `server_name` | `""` | Override SNI and expected certificate hostname (defaults to `host`) |
| `client_cert_file` | `""` | Client PEM certificate path for mTLS |
| `client_key_file` | `""` | Client private key path for mTLS |
| `cipher_list` | `""` | OpenSSL cipher list (applied on TCP / WebSocket clients) |

```cpp
cpphttp::HttpClient client("127.0.0.1", 8443);

// Option 1: Verify against explicit CA / pinned cert (Recommended)
cpphttp::ClientTlsConfig tls;
tls.ca_file = "certs/server.crt";
tls.server_name = "localhost"; // override if SAN differs from connect host
client.EnableSSL(tls);

// Option 2: Explicitly opt out of verification for local dev self-signed certs
// client.EnableSSL();
// client.SetAllowSelfSigned(true);
```

> **Warning:** `SetAllowSelfSigned(true)` sets `verify_peer = false`, disabling certificate and hostname validation. Use `ca_file` in production or whenever testing with a known self-signed cert.

### Redirect Behavior and Cross-Origin Scoping

`HttpClientImpl` automatically follows `3xx` responses with a `Location` header (up to `SetMaxRedirects(int)`, default `5`) with strict security scoping:

1. **Method rules:** `301`, `302`, and `303` downgrade `POST`/`PUT`/etc. to `GET` and clear the request body. `307` and `308` preserve the original HTTP method and body.
2. **Scheme filtering:** Only relative paths (`/path`), protocol-relative URLs (`//host/path`), `http://`, and `https://` targets are followed.
3. **No HTTPS → HTTP downgrade:** If the client is on HTTPS (`ssl_enabled_ == true`) and `Location` points to `http://`, the client refuses the downgrade and returns the `3xx` response directly.
4. **Cross-origin credential & TLS scoping:** When redirecting to a different host, port, or scheme, `HttpClientImpl` opens a temporary client for the target origin and:
   - Strips `Authorization`, `Proxy-Authorization`, and `Cookie` headers (`StripCredentialHeaders`).
   - Copies CA/verification policy (`verify_peer`, `ca_file`, `ca_path`, `cipher_list`) **while clearing `server_name`, `client_cert_file`, and `client_key_file`** so mTLS client certificates and custom SNI names are never sent to third-party origins.

---

## HTTP/3 and Hybrid Transports

When compiled with `CPPHTTP_ENABLE_QUIC` (enabled by default in `CMakeLists.txt`), `cpp-http` provides HTTP/3 (HTTP-over-QUIC) via `cpp-quic`.

### Transport Aliases

| Alias / Class | Underlying Transport | Protocol / Sockets |
| :--- | :--- | :--- |
| `cpphttp::HttpServer` | `TcpServerTransport` | TCP (HTTP/1.1 & HTTPS) |
| `cpphttp::HttpClient` | `TcpClientTransport` | TCP (HTTP/1.1 & HTTPS client) |
| `cpphttp::QuicHttpServer` | `QuicServerTransport` | UDP/QUIC (HTTP/3 & raw HTTP-over-QUIC) |
| `cpphttp::HybridHttpServer` | `HybridServerTransport` | TCP (HTTPS) + UDP (HTTP/3) on the same port |
| `cpphttp::HttpClientImpl<cpphttp::QuicClientTransport>` | `QuicClientTransport` | UDP/QUIC client |

### Hybrid HTTP/3 Server

`HybridHttpServer` binds both a TCP listener and a QUIC/UDP server to the same port, routing both protocols through one shared route table and middleware pipeline. By default, TCP responses automatically include `Alt-Svc: h3=":<port>"; ma=86400` (`server.SetAdvertiseAltSvc(bool)`, default `true`).

```cpp
uint16_t port = 4433;
cpphttp::HybridHttpServer server(port);

// 1. Configure TLS on the TCP listener
cpptcpnet::TcpListener::SslConfig ssl_config;
ssl_config.cert_file = "certs/server.crt";
ssl_config.key_file = "certs/server.key";
server.GetTransport()->GetTcpTransport().GetListener().EnableSSL(ssl_config);

// 2. Configure certificates (and ALPN / QuicProfile) on the QUIC server
server.GetTransport()->GetQuicTransport().GetServer().SetCertificate(
    "certs/server.crt", "certs/server.key"
);
server.GetTransport()->GetQuicTransport().GetServer()
    .SetAlpnProtos({"h3", "h3-29"});

cppquic::QuicProfile qprofile = cppquic::QuicProfile::HighThroughput();
server.SetQuicProfile(qprofile);

// 3. Register routes shared by both TCP and QUIC
server.Get("/hello", [](const cpphttp::HttpRequest& req) {
    return cpphttp::HttpResponse::Plain("Hello over HTTPS or HTTP/3!");
});

server.Start();
```

> **Note:** `HybridServerTransport` does not expose `GetListener()` directly on `HybridHttpServer` (unlike `HttpServer`). Access the TCP listener via `server.GetTransport()->GetTcpTransport().GetListener()` and the QUIC server via `server.GetTransport()->GetQuicTransport().GetServer()` (or `server.GetTransport()->GetServer()`).

### HTTP/3 Client (`QuicClientTransport`)

```cpp
cpphttp::HttpClientImpl<cpphttp::QuicClientTransport> client("127.0.0.1", 4433);
cpphttp::ClientTlsConfig tls;
tls.ca_file = "certs/server.crt";
client.EnableSSL(tls);
client.SetTimeout(std::chrono::seconds(5));
client.SetQuicProfile(cppquic::QuicProfile::HighThroughput());

cpphttp::HttpResponse res = client.Get("/hello");
```

> **Tip:** When you call `SetQuicProfile(profile)` with `profile.initial_max_data >= 32 * 1024 * 1024`, `QuicServerTransport` and `QuicClientTransport` automatically raise `send_chunk_size` to `128 KB` to match high-throughput QUIC flow-control windows.

### Low-Level HTTP/3 and QPACK Helpers

`cpp-http` exposes RFC 9114 / RFC 9204 framing primitives under `cpphttp::qpack` and `cpphttp::http3`:

- `cpphttp::qpack`: `Encode`, `Decode`, `EncodeInteger`, `DecodeInteger`, `EncodeString`, `DecodeString`, `DecodeHuffman`, `GetStaticTable`.
- `cpphttp::http3`: `EncodeVarint`, `DecodeVarint`, `EncodeFrame`, `GetSettingsPayload`, `ReconstructHttp11Request`, `ReconstructHttp11Response`, `ParseHttp11Response`.

---

## Limits and DoS Protection

All three of `HttpServerImpl`, `HttpClientImpl`, and `WebSocketClient` enforce strict size, timeout, and framing checks by default.

### Default Limits Reference

| Component | Setter / Getter | Default | Behavior on Violation |
| :--- | :--- | :--- | :--- |
| `HttpServerImpl` | `SetMaxHeaderSize` / `GetMaxHeaderSize` | `8192` bytes (8 KB) | Sends `431 Request Header Fields Too Large` + closes connection |
| `HttpServerImpl` | `SetMaxBodySize` / `GetMaxBodySize` | `10 * 1024 * 1024` (10 MB) | Sends `413 Payload Too Large` + closes connection (closes QUIC conn on H3) |
| `HttpServerImpl` | `SetIdleTimeout` / `GetIdleTimeout` | `10ms` | Wait interval on `background_thread_` event loop when idle |
| `HttpServerImpl` | `SetSendChunkSize` / `SetMaxOutboundBufferSize` | Transport defaults (`64 KB` / `10 MB` TCP, `1 MB` QUIC) | Controls outbound chunk size and backpressure threshold |
| `HttpClientImpl` | `SetTimeout` / `GetTimeout` | `10000ms` (10 s) | Throws `std::runtime_error` on connect or response timeout |
| `HttpClientImpl` | `SetMaxHeaderSize` / `GetMaxHeaderSize` | `8192` bytes (8 KB) | Logs error and aborts connection |
| `HttpClientImpl` | `SetMaxBodySize` / `GetMaxBodySize` | `10 * 1024 * 1024` (10 MB) | Aborts oversized buffered/decompressed responses |
| `HttpClientImpl` | `SetMaxRedirects` / `GetMaxRedirects` | `5` | Throws `std::runtime_error("Too many redirects")` |
| `WebSocketClient` | `SetMaxBodySize` / `GetMaxBodySize` | `10 * 1024 * 1024` (10 MB) | Closes with `1009 Message too big` |
| `WebSocketClient` | `SetMaxOutboundBufferSize` / `GetMaxOutboundBufferSize` | `10 * 1024 * 1024` (10 MB) | Caps `cpptcpnet::TcpClient` outbound queue |
| `WebSocketClient` | `SetCloseTimeout` / `GetCloseTimeout` | `1000ms` (1 s) | Max wait for peer close frame on `Close(1000)` |

The HTTP/1.1 parser also strictly rejects request smuggling vectors: duplicate `Content-Length` headers, non-digit `Content-Length` values (`ParseContentLength`), header names containing whitespace, non-`chunked` or multi-valued `Transfer-Encoding`, invalid chunk size lines (`ParseChunkSizeLine`), and `CR`/`LF` injection in serialized headers or HTTP/3 pseudo-headers.

---

## Multi-Threaded Safety and Logging

### Thread-Safe Handlers and Client Worker Pool

- **Server:** `HttpServer` processes connections on `cpptcpnet`'s session-affinity worker pool (`session_id % num_workers`). Packets for one TCP connection are always serialized on its assigned worker thread, while different connections run concurrently across CPU cores. Always protect shared mutable state (maps, counters, DB handles) with `std::mutex` or `std::atomic`.
- **Client:** `HttpClient` serializes concurrent synchronous calls on the same instance with `request_mutex_`. Asynchronous calls (`GetAsync`, `PostAsync`, `PutAsync`, `DeleteAsync`, `PostStreamAsync`, `PutStreamAsync`) run on a shared global `cppasyncworker::WorkerPool` (size configurable via `cpphttp::SetClientWorkerPoolSize(size_t)`, defaulting to `std::thread::hardware_concurrency()`, minimum `4` when `<= 1`), or a custom pool passed to the `HttpClient` constructor.

### Version and Internal Library Logging

Query the library version with `cpphttp::version()` (returns `"MAJOR.MINOR.PATCH"`, e.g. `"1.13.0"`).

Internal diagnostics from `HttpClient`, `WebSocketClient`, and `QuicServerTransport` are silent by default. Register a global callback via `cpphttp::SetLogger`:

```cpp
cpphttp::SetLogger([](cpphttp::LogSeverity severity,
                      const std::string& className,
                      const std::string& message) {
    const char* level = "INFO";
    if (severity == cpphttp::LogSeverity::Warning) level = "WARNING";
    if (severity == cpphttp::LogSeverity::Error) level = "ERROR";
    std::cout << "[HTTP-LIB][" << level << "][" << className << "] "
              << message << "\n";
});

// Pass nullptr to disable logging
cpphttp::SetLogger(nullptr);
```

`cpphttp::LogSeverity` values are `Info`, `Warning`, and `Error`.

---

## TCP Defragmentation and Pipelining

TCP delivers an unstructured byte stream where HTTP requests may arrive split across multiple `recv` events (fragmentation) or coalesced back-to-back in a single buffer (HTTP pipelining):

1. **Fragmentation Protection:** `HttpServerImpl` appends incoming bytes into a per-session `std::deque<uint8_t>` (`SessionState::buffer`) and searches for `\r\n\r\n`. Until the complete headers and either `Content-Length` bytes or the terminating `0\r\n\r\n` chunk arrive, routing is deferred while waiting for subsequent packets.
2. **Pipelining Support:** After dispatching a request and sending the response, the server erases only `[0, body_start_idx + consumed_body_bytes)` from `session->buffer` and loops, immediately processing any subsequent pipelined requests already buffered. If the connection is closing (`Connection: close`, HTTP/1.0 without `keep-alive`, or early stream abort), `discard_input` is set and buffered tail bytes are discarded.
3. **Client Framing Resilience:** `HttpClientImpl::ReadResponse` automatically discards `1xx` interim responses (such as `100 Continue` and `103 Early Hints`) before parsing the final response, and supports HTTP/1.0 EOF-delimited bodies when both `Content-Length` and `Transfer-Encoding` are absent.

---

<div class="page-nav">
  <a href="./basic-usage.html" class="page-nav-link">
    <span class="page-nav-subtitle">Previous Section</span>
    <span class="page-nav-title">&larr; Basic Usage</span>
  </a>
  <a href="./architecture-and-examples.html" class="page-nav-link page-nav-next">
    <span class="page-nav-subtitle">Next Section</span>
    <span class="page-nav-title">Architecture & Examples &rarr;</span>
  </a>
</div>
