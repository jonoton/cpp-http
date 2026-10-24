#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>
#include "cpphttp.hpp"

namespace {
std::atomic<bool> g_keep_running{true};
void SignalHandler(int) {
  g_keep_running.store(false, std::memory_order_relaxed);
}
}  // namespace

int main(int argc, char *argv[]) {
  // Disable stdout buffering for immediate log outputs
  std::setvbuf(stdout, NULL, _IONBF, 0);

  uint16_t port = 8080;
  size_t worker_threads = std::thread::hardware_concurrency();
  if (worker_threads <= 1) {
    worker_threads = 4;
  }

  if (argc > 1) {
    try {
      int p = std::stoi(argv[1]);
      if (p > 0 && p <= 65535) {
        port = static_cast<uint16_t>(p);
      } else {
        std::cerr << "Port out of range (1-65535). Using default 8080.\n";
      }
    } catch (...) {
      std::cerr << "Invalid port argument. Using default 8080.\n";
    }
  }
  if (argc > 2) {
    try {
      int t = std::stoi(argv[2]);
      if (t > 0) {
        worker_threads = static_cast<size_t>(t);
      } else {
        std::cerr << "Thread count must be positive. Using default: " << worker_threads << "\n";
      }
    } catch (...) {
      std::cerr << "Invalid threads argument. Using default: " << worker_threads << "\n";
    }
  }

  // Register signal handlers for clean shutdown
  ::signal(SIGINT, SignalHandler);
  ::signal(SIGTERM, SignalHandler);

  // Instantiate standard HTTP server
  cpphttp::HttpServer server(port, "0.0.0.0");

  // Configure the number of background worker threads in the transport layer.
  // TcpListener::SetWorkerThreadCount() controls how many threads handle data
  // callbacks, with session affinity ensuring in-order processing per session.
  // Must be called before Start().
  server.GetListener().SetWorkerThreadCount(worker_threads);

  // Endpoint 1: GET / - Plaintext hello
  server.Get("/", [](const cpphttp::HttpRequest &req) {
    return cpphttp::HttpResponse::Plain("Hello, World!");
  });

  // Endpoint 2: POST /echo - Echo body content
  server.Post("/echo", [](const cpphttp::HttpRequest &req) {
    cpphttp::HttpResponse res;
    res.status_code = 200;
    res.status_message = "OK";
    std::string ct = req.GetHeader("Content-Type");
    res.headers["Content-Type"] = ct.empty() ? "text/plain" : std::move(ct);
    res.body = req.body;
    return res;
  });

  // Endpoint 3: GET /user/:id - JSON path param matching
  server.Get("/user/:id", [](const cpphttp::HttpRequest &req) {
    auto it = req.path_params.find("id");
    std::string user_id = (it != req.path_params.end()) ? it->second : "";
    return cpphttp::HttpResponse::Json("{\"id\":\"" + user_id + "\"}");
  });

  // Global Middleware for tracing and header manipulation overhead
  server.Use([](cpphttp::HttpRequest &req, cpphttp::HttpResponse &res) {
    res.headers["X-Benchmark-Processed"] = "true";
    return true; // continue routing
  });

  // Endpoint 4: GET /search - Query parameter parsing
  server.Get("/search", [](const cpphttp::HttpRequest &req) {
    auto get_param = [&](const char *key, const char *def) -> std::string {
      auto it = req.query_params.find(key);
      return (it != req.query_params.end()) ? it->second : def;
    };
    std::string query = get_param("query", "");
    std::string limit = get_param("limit", "10");
    std::string offset = get_param("offset", "0");
    std::string sort = get_param("sort", "asc");
    return cpphttp::HttpResponse::Json("{\"query\":\"" + query + "\",\"limit\":" + limit + ",\"offset\":" + offset + ",\"sort\":\"" + sort + "\"}");
  });

  // Endpoint 5: GET /data - Large payload and transparent compression
  server.Get("/data", [](const cpphttp::HttpRequest &req) {
    static std::string large_json = []() {
      std::string s = "{\"status\":\"success\",\"items\":[";
      for (int i = 0; i < 40; ++i) {
        if (i > 0) s += ",";
        s += "{\"id\":" + std::to_string(i) + 
             ",\"name\":\"item_" + std::to_string(i) + 
             "\",\"description\":\"This is a longer description string for mock data item number " + std::to_string(i) + 
             " to ensure that the payload exceeds the minimum threshold for compression.\"}";
      }
      s += "]}";
      return s;
    }();
    return cpphttp::HttpResponse::Json(large_json);
  });

  // Endpoint 6: WebSocket /ws - WebSocket Echo handler
  cpphttp::WebSocketBehavior ws_behavior;
  ws_behavior.on_message = [](std::shared_ptr<cpphttp::WebSocketConnection> conn, const std::string &msg) {
    conn->Send(msg);
  };
  server.WebSocket("/ws", std::move(ws_behavior));



  std::cout << "Starting benchmark server on port " << port << " with " << worker_threads << " worker threads...\n";

  try {
    server.Start();
    std::cout << "Server Running" << std::endl;

    while (g_keep_running.load(std::memory_order_relaxed)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    server.Stop();
  } catch (const std::exception &e) {
    std::cerr << "Server Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
