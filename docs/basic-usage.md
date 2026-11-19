---
layout: default
title: Basic Usage
description: Core routing, request handling, parameter extraction, client usage, and error handling in cpp-http.
---

# Basic Usage

`cpp-http` provides an intuitive, high-performance C++17 API for defining server routes and executing client requests. This guide covers core routing, parameter extraction, structured response builders, headers, the synchronous and asynchronous client, and comprehensive error handling.

---

## Table of Contents

1. [Starting the Server](#1-starting-the-server)
2. [HTTP Routing & Methods](#2-http-routing--methods)
3. [Path and Query Parameters](#3-path-and-query-parameters)
4. [The HttpRequest Object](#4-the-httprequest-object)
5. [The HttpResponse Object & Static Builders](#5-the-httpresponse-object--static-builders)
6. [Header Normalization & Multi-Headers](#6-header-normalization--multi-headers)
7. [The HTTP Client (Sync & Async)](#7-the-http-client-sync--async)
8. [Error Handling Patterns](#8-error-handling-patterns)

---

## 1. Starting the Server

The `cpphttp::HttpServer` class manages listening sockets, the background event loop, and session-affinity worker threads.

```cpp
#include "cpphttp.hpp"
#include <iostream>

int main() {
    // 1. Construct server listening on port 8080 (binds to 0.0.0.0 by default)
    cpphttp::HttpServer server(8080);

    // 2. Register a basic route
    server.Get("/", [](const cpphttp::HttpRequest& req) {
        return cpphttp::HttpResponse::Plain("Server is running!");
    });

    // 3. Start the background event loop
    try {
        server.Start();
        std::cout << "HTTP Server running on http://localhost:8080\n";
        std::cout << "Press Enter to stop...\n";
        std::cin.get();
        server.Stop();
    } catch (const std::exception& e) {
        std::cerr << "Startup failure: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
```

### Binding to Specific IP Addresses

By default, the server binds to `0.0.0.0` (all available interfaces). You can restrict listening to a specific interface (e.g. `127.0.0.1` for local development):

```cpp
// Bind exclusively to loopback interface on port 8080
cpphttp::HttpServer server(8080, "127.0.0.1");
```

---

## 2. HTTP Routing & Methods

`HttpServer` provides dedicated registration methods for all standard HTTP verbs:

```cpp
// GET request handler
server.Get("/items", [](const cpphttp::HttpRequest& req) {
    return cpphttp::HttpResponse::Json("[{\"id\": 1}, {\"id\": 2}]");
});

// POST request handler
server.Post("/items", [](const cpphttp::HttpRequest& req) {
    std::cout << "Creating item with body: " << req.body << "\n";
    return cpphttp::HttpResponse::Json("{\"status\": \"created\"}", 201);
});

// PUT request handler
server.Put("/items/:id", [](const cpphttp::HttpRequest& req) {
    std::string id = req.path_params.at("id");
    return cpphttp::HttpResponse::Plain("Updated item " + id);
});

// PATCH request handler
server.Patch("/items/:id", [](const cpphttp::HttpRequest& req) {
    return cpphttp::HttpResponse::Plain("Patched item");
});

// DELETE request handler
server.Delete("/items/:id", [](const cpphttp::HttpRequest& req) {
    return cpphttp::HttpResponse::Plain("Item deleted", 200);
});

// OPTIONS request handler (useful for CORS)
server.Options("/items", [](const cpphttp::HttpRequest& req) {
    cpphttp::HttpResponse res;
    res.status_code = 204;
    res.headers["Allow"] = "GET, POST, OPTIONS";
    return res;
});

// HEAD request handler
server.Head("/status", [](const cpphttp::HttpRequest& req) {
    cpphttp::HttpResponse res;
    res.status_code = 200;
    res.headers["Content-Length"] = "0";
    return res;
});
```

> **Note:** If a client sends a `HEAD` request to a route that only has a `GET` handler registered, `cpp-http` automatically executes the `GET` handler and strips the body from the final response, preserving status code and headers.

### Static Directory Routing

To serve local directories of files (HTML, CSS, JS, images, media):

```cpp
// Serves static files under URL prefix /assets/ from disk folder ./public
server.StaticDir("/assets", "./public");

// SPA (Single Page Application) mode:
// Requests that do not match an asset fall back to serving index.html
server.StaticDir("/dashboard", "./dist", /*spa_mode=*/true);
```

---

## 3. Path and Query Parameters

`cpp-http` automatically segments paths, maps route variables, normalizes query parameters, and performs percent-decoding.

### Route Variables (`:param`)

Segments prefixed with `:` capture dynamic values into `req.path_params`:

```cpp
server.Get(
    "/users/:userId/orders/:orderId",
    [](const cpphttp::HttpRequest& req) {
        std::string user_id = req.path_params.at("userId");
        std::string order_id = req.path_params.at("orderId");

        return cpphttp::HttpResponse::Plain(
            "User: " + user_id + ", Order: " + order_id
        );
    });
```

### Wildcard Matching (`*`)

Use an asterisk `*` to capture all trailing path segments:

```cpp
server.Get("/files/*", [](const cpphttp::HttpRequest& req) {
    // Access the captured wildcard path via the "*" key
    std::string relative_path = req.path_params.at("*");
    return cpphttp::HttpResponse::Plain("File requested: " + relative_path);
});
```

### Query Parameters

Query strings (e.g. `?search=c%2B%2B&page=2`) are automatically separated from `req.path`, percent-decoded, and populated in `req.query_params`:

```cpp
server.Get("/search", [](const cpphttp::HttpRequest& req) {
    // Safe lookup helper
    std::string query = req.query_params.count("q")
        ? req.query_params.at("q") : "";
    std::string page = req.query_params.count("page")
        ? req.query_params.at("page") : "1";

    return cpphttp::HttpResponse::Json(
        "{\"query\": \"" + query + "\", \"page\": " + page + "}"
    );
});
```

---

## 4. The HttpRequest Object

Each route callback receives a `const cpphttp::HttpRequest&` representing the decoded HTTP request:

```cpp
struct HttpRequest {
    std::string method;  // HTTP method (e.g. "GET", "POST")
    std::string path;    // Clean path without query string
    std::string version; // Protocol version (e.g. "HTTP/1.1")
    HeaderMap headers;   // Case-insensitive header map
    MultiHeaderMap multi_headers; // Multi-headers (duplicate keys)
    std::string body;    // Raw request payload body
    std::unordered_map<std::string, std::string> query_params;
    std::unordered_map<std::string, std::string> path_params;
    std::string client_ip; // Peer socket address

    // Case-insensitive header lookup helper; returns "" if not found
    std::string GetHeader(const std::string &key) const;
};
```

---

## 5. The HttpResponse Object & Static Builders

Handlers return a `cpphttp::HttpResponse`. You can construct responses manually or utilize ergonomic static builders:

```cpp
struct HttpResponse {
    int status_code = 200;
    std::string status_message = "OK";
    HeaderMap headers;            // Case-insensitive response headers
    MultiHeaderMap multi_headers; // Multimap for duplicate keys (e.g. cookies)
    std::string body;             // Response payload body

    // Static Ergonomic Response Builders
    static HttpResponse Json(const std::string &json, int status = 200);
    static HttpResponse Html(const std::string &html, int status = 200);
    static HttpResponse Plain(const std::string &text, int status = 200);
    static HttpResponse Redirect(const std::string &url, int status = 302);
};
```

### Examples of Static Builders

```cpp
// 1. Return JSON with automatic Content-Type: application/json
server.Get("/api/health", [](const cpphttp::HttpRequest& req) {
    return cpphttp::HttpResponse::Json(
        "{\"status\":\"healthy\",\"uptime\":3600}");
});

// 2. Return HTML with automatic Content-Type: text/html
server.Get("/welcome", [](const cpphttp::HttpRequest& req) {
    return cpphttp::HttpResponse::Html("<h1>Hello World</h1>");
});

// 3. Return Plain Text with automatic Content-Type: text/plain
server.Get("/ping", [](const cpphttp::HttpRequest& req) {
    return cpphttp::HttpResponse::Plain("pong");
});

// 4. HTTP Redirection (301, 302, 307, 308)
server.Get("/old-path", [](const cpphttp::HttpRequest& req) {
    return cpphttp::HttpResponse::Redirect("/new-path", 301);
});
```

---

## 6. Header Normalization & Multi-Headers

### Case-Insensitive Matching

HTTP specification (RFC 7230) dictates that header field names are case-insensitive. `cpp-http` uses custom case-insensitive hashers so lookups succeed regardless of casing:

```cpp
server.Post("/upload", [](const cpphttp::HttpRequest& req) {
    // Both retrieve the header accurately:
    std::string auth1 = req.GetHeader("Authorization");
    std::string auth2 = req.GetHeader("authorization");

    cpphttp::HttpResponse res = cpphttp::HttpResponse::Plain("Uploaded");
    res.headers["X-Custom-Header"] = "value123";
    return res;
});
```

### Multi-Headers (e.g. `Set-Cookie`)

Some headers like `Set-Cookie` can appear multiple times with different values. `HttpResponse::multi_headers` is an `unordered_multimap` designed for this:

```cpp
server.Get("/login", [](const cpphttp::HttpRequest& req) {
    cpphttp::HttpResponse res = cpphttp::HttpResponse::Plain("Logged in");
    
    // Set multiple cookies in a single response
    res.multi_headers.insert(
        {"Set-Cookie", "session_id=abc123; Secure; HttpOnly"});
    res.multi_headers.insert(
        {"Set-Cookie", "theme=dark; Path=/; Max-Age=31536000"});
    
    return res;
});
```

---

## 7. The HTTP Client (Sync & Async)

`cpphttp::HttpClient` supports both synchronous (blocking) and asynchronous (future-based non-blocking) requests.

### Synchronous (Blocking) Client

```cpp
#include "cpphttp.hpp"
#include <iostream>

int main() {
    cpphttp::HttpClient client("127.0.0.1", 8080);
    client.SetTimeout(std::chrono::seconds(5)); // Timeout after 5s

    try {
        // GET request
        cpphttp::HttpResponse res = client.Get("/items");
        std::cout << "Status: " << res.status_code << "\n";
        std::cout << "Body: " << res.body << "\n";

        // POST request with custom headers
        std::unordered_map<std::string, std::string> headers = {
            {"Content-Type", "application/json"}
        };
        cpphttp::HttpResponse post_res = client.Post(
            "/items", "{\"name\":\"Widget\"}", headers);
        std::cout << "POST Status: " << post_res.status_code << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Client error: " << e.what() << "\n";
    }

    return 0;
}
```

### Asynchronous (Non-Blocking) Client

Asynchronous requests return a `std::future<HttpResponse>` and execute in a background thread pool managed by `cpp-asyncworker`:

```cpp
#include "cpphttp.hpp"
#include <iostream>
#include <future>

int main() {
    cpphttp::HttpClient client("127.0.0.1", 8080);

    // Launch non-blocking request
    std::future<cpphttp::HttpResponse> future = client.GetAsync("/items/42");

    // Perform other computational or I/O work concurrently...

    // Block only when ready to consume the response
    cpphttp::HttpResponse res = future.get();
    std::cout << "Result: " << res.body << "\n";

    return 0;
}
```

### Connection Keep-Alive & Redirects

- **Connection Persistence:** `HttpClient` automatically preserves the underlying TCP socket across consecutive requests (`Connection: keep-alive`). If the connection drops or server replies with `Connection: close`, it cleanly reconnects on the next call.
- **Redirect Following:** Automatically follows `301`, `302`, `303`, `307`, and `308` redirects up to a maximum count (default is `5`, configurable via `client.SetMaxRedirects(10)`).

---

## 8. Error Handling Patterns

### Server Route Not Found (404)

If an incoming request does not match any registered routes or static directory files, `HttpServer` automatically constructs and sends a `404 Not Found` response:

```http
HTTP/1.1 404 Not Found
Content-Type: text/plain
Content-Length: 22

404 - Route not found.
```

### Handler Error Responses

To return error statuses from within your handlers, set `status_code` and `status_message`, or use static builders with the appropriate HTTP error code:

```cpp
server.Get("/account/:id", [](const cpphttp::HttpRequest& req) {
    std::string id = req.path_params.at("id");

    if (id == "admin") {
        return cpphttp::HttpResponse::Json("{\"error\":\"Forbidden\"}", 403);
    }

    if (id != "123") {
        return cpphttp::HttpResponse::Json(
            "{\"error\":\"Account not found\"}", 404);
    }

    return cpphttp::HttpResponse::Json("{\"account_id\":123,\"balance\":2500}");
});
```

### Client Exception Handling

`HttpClient` throws `std::runtime_error` when network-level failures occur (e.g. connection refused, network timeouts, or invalid remote hostnames). Always wrap client calls in a `try / catch` block:

```cpp
cpphttp::HttpClient client("127.0.0.1", 8080);
client.SetTimeout(std::chrono::milliseconds(2500));

try {
    cpphttp::HttpResponse res = client.Get("/api/data");
    
    // Handle application-level HTTP errors
    if (res.status_code >= 400) {
        std::cerr << "Server returned HTTP error " << res.status_code
                  << ": " << res.body << "\n";
    } else {
        std::cout << "Success: " << res.body << "\n";
    }
} catch (const std::exception& e) {
    // Network-level failure (timeout, DNS resolution, socket disconnect)
    std::cerr << "Network transport error: " << e.what() << "\n";
}
```

---

<div class="page-nav">
  <a href="./getting-started.html" class="page-nav-link">
    <span class="page-nav-subtitle">Previous Section</span>
    <span class="page-nav-title">&larr; Getting Started</span>
  </a>
  <a href="./advanced-usage.html" class="page-nav-link page-nav-next">
    <span class="page-nav-subtitle">Next Section</span>
    <span class="page-nav-title">Advanced Usage &rarr;</span>
  </a>
</div>
