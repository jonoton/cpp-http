---
layout: default
---

[< Previous: Getting Started](./getting-started.html) | [🏠 Home](./) | [Next: Advanced Usage >](./advanced-usage.html)
<hr>

# Basic Usage

The `HttpServer` class allows you to create a high-performance, asynchronous HTTP/1.1 server. It handles incoming connections, parses HTTP packets, and routes them to user-defined route handlers.

## Starting the Server

Here is a minimal example demonstrating how to construct a server, register simple `GET` and `POST` routes, and start listening for HTTP requests:

```cpp
#include "cpphttp.hpp"
#include <iostream>

int main() {
    // Construct HttpServer listening on port 8080
    cpphttp::HttpServer server(8080);

    // Register a basic GET route
    server.Get("/", [](const cpphttp::HttpRequest& request) {
        cpphttp::HttpResponse response;
        response.status_code = 200;
        response.status_message = "OK";
        response.headers["Content-Type"] = "text/html";
        response.body = "<h1>Welcome to cpp-http!</h1>";
        return response;
    });

    // Register a POST route to accept client data
    server.Post("/submit", [](const cpphttp::HttpRequest& request) {
        std::cout << "POST content: " << request.body << "\n";

        cpphttp::HttpResponse response;
        response.status_code = 201;
        response.status_message = "Created";
        response.headers["Content-Type"] = "text/plain";
        response.body = "Submission successful!";
        return response;
    });

    // Start the server background loop
    try {
        server.Start();
        std::cout << "HTTP Server running on port 8080. Press Enter to stop.\n";
        std::cin.get();
        server.Stop();
    } catch (const std::exception& e) {
        std::cerr << "Failed to start HTTP server: " << e.what() << "\n";
    }

    return 0;
}
```

### Custom Bind Address

By default, the server binds to all available network interfaces (`0.0.0.0`). You can restrict the server to a specific interface by providing an optional second parameter to the constructor:

```cpp
// Bind only to localhost / loopback interface (local development)
cpphttp::HttpServer server(8080, "127.0.0.1");
```

## The HttpRequest Structure

The server maps incoming requests to a structured `HttpRequest` object:

```cpp
// Case-insensitive maps used for headers
using HeaderMap = std::unordered_map<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEqual>;
using MultiHeaderMap = std::unordered_multimap<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEqual>;

struct HttpRequest
{
    std::string method;                                        // e.g., "GET", "POST"
    std::string path;                                          // stripped of query variables, e.g., "/profile"
    std::string version;                                       // e.g., "HTTP/1.1"
    HeaderMap headers;                                         // Case-insensitive HTTP Header Map
    MultiHeaderMap multi_headers;                              // Multi-headers map (allows duplicate keys like Set-Cookie)
    std::string body;                                          // Request payload
    std::string client_ip;                                     // Client IP address (from X-Forwarded-For or Peer Address)
    std::unordered_map<std::string, std::string> query_params; // Query params (e.g. ?name=val)
    std::unordered_map<std::string, std::string> path_params;  // Dynamically matched segments (e.g. :id)

    // Lookup headers case-insensitively
    std::string GetHeader(const std::string &key) const;
};
```

When a request is received, headers are automatically parsed, query parameters are separated and decoded, and path parameters are matched. Headers in `HttpRequest::headers` are case-insensitive, meaning `request.headers["Content-Type"]` and `request.headers["content-type"]` refer to the same value.

## The HttpResponse Structure

Route handlers return a `HttpResponse` structure:

```cpp
struct HttpResponse
{
    int status_code = 200;
    std::string status_message = "OK";
    HeaderMap headers;            // Case-insensitive HTTP Header Map
    MultiHeaderMap multi_headers; // Multi-headers map (allows duplicate keys like Set-Cookie)
    std::string body;

    // Lookup headers case-insensitively
    std::string GetHeader(const std::string &key) const;

    // Static Response Builders
    static HttpResponse Json(const std::string &json_body, int status_code = 200);
    static HttpResponse Html(const std::string &html_body, int status_code = 200);
    static HttpResponse Plain(const std::string &text_body, int status_code = 200);
    static HttpResponse Redirect(const std::string &location, int status_code = 302);

    // Serializes the response object into a valid HTTP/1.1 raw string.
    std::string Serialize() const;
};
```

### Static Response Builders
To simplify returning responses, you can use the static helpers inside route callbacks:
```cpp
server.Get("/api/data", [](const cpphttp::HttpRequest& req) {
    return cpphttp::HttpResponse::Json("{\"status\":\"ok\"}");
});
```

---

## Performing Client Requests

`cpp-http` includes a client `HttpClient` supporting both synchronous (blocking) and asynchronous (non-blocking) request execution.

### Synchronous Requests

```cpp
#include "cpphttp.hpp"
#include <iostream>

int main() {
    // Construct client
    cpphttp::HttpClient client("127.0.0.1", 8080);
    client.SetMaxBodySize(10 * 1024 * 1024); // Protect against large response bodies (10MB)

    try {
        // Send a GET request
        cpphttp::HttpResponse response = client.Get("/api/data");
        std::cout << "Status: " << response.status_code << "\n";
        std::cout << "Body: " << response.body << "\n";
    } catch (const std::exception &e) {
        std::cerr << "Request failed: " << e.what() << "\n";
    }

    return 0;
}
```

### Connection Persistence & Keep-Alive

`HttpClient` automatically manages the lifetime of its underlying TCP connection. By default, it uses `Connection: keep-alive` to keep the connection open for subsequent requests, reducing connection handshake overhead.
* If a server replies with `Connection: close` (or standard connection failures occur), the client automatically tears down and rebuilds the connection on the next request.
* If multiple threads call requests concurrently on a single `HttpClient` instance, requests are safely serialized using an internal mutex.

### Automatic Redirect Following

When the server replies with redirection codes (`301`, `302`, etc.), the client automatically parses the `Location` header and follows the redirect (up to a depth of 5).
* Redirection to relative paths or the same host/port automatically reuses the existing persistent connection.
* Redirection to external hosts spawns a temporary client under-the-hood to complete the request.

### Asynchronous Requests (Using cpp-asyncworker)

The client also provides asynchronous counterparts (e.g. `GetAsync`, `PostAsync`, `PutAsync`, `DeleteAsync`) which return a `std::future<HttpResponse>` executing concurrently in a background thread pool:

```cpp
#include "cpphttp.hpp"
#include <iostream>
#include <future>

int main() {
    cpphttp::HttpClient client("127.0.0.1", 8080);

    try {
        // Send a GET request asynchronously
        std::future<cpphttp::HttpResponse> future = client.GetAsync("/api/data");

        // ... do other work while the network request runs in the background ...

        cpphttp::HttpResponse response = future.get(); // Blocks until response is received
        std::cout << "Status: " << response.status_code << "\n";
        std::cout << "Body: " << response.body << "\n";
    } catch (const std::exception &e) {
        std::cerr << "Request failed: " << e.what() << "\n";
    }

    return 0;
}
```

---
[🏠 Home](./) | [Next: Advanced Usage >](./advanced-usage.html)
