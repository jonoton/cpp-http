#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

#include "cpphttp.hpp"

std::string FindCertFile(const std::string &filename) {
  // 1. Try current working directory
  if (std::filesystem::exists(filename)) {
    return filename;
  }
  // 2. Try certs/filename
  std::string path = "certs/" + filename;
  if (std::filesystem::exists(path)) {
    return path;
  }
  // 3. Try parent directories (for build/Debug or build-windows/Debug etc.)
  path = "../certs/" + filename;
  if (std::filesystem::exists(path)) {
    return path;
  }
  path = "../../certs/" + filename;
  if (std::filesystem::exists(path)) {
    return path;
  }
  path = "../../../certs/" + filename;
  if (std::filesystem::exists(path)) {
    return path;
  }
  // 4. Try compile-time configured directory
#ifdef CPPHTTP_CERTS_DIR
  path = std::string(CPPHTTP_CERTS_DIR) + "/" + filename;
  if (std::filesystem::exists(path)) {
    return path;
  }
#endif
  // 5. Try hardcoded Dev Container path as fallback
  path = "/workspaces/cpp-http/certs/" + filename;
  if (std::filesystem::exists(path)) {
    return path;
  }
  // If nothing else, return the default
  return "certs/" + filename;
}

int main(int argc, char *argv[]) {
  uint16_t port = 4433;
  if (argc > 1) {
    try {
      port = static_cast<uint16_t>(std::stoi(argv[1]));
    } catch (...) {
      std::cerr << "Invalid port argument. Using default 4433." << std::endl;
    }
  }

  try {
    // Enable logging for the QUIC transport layer
    cppquic::SetLogger([](cppquic::LogSeverity severity,
                          const std::string &className,
                          const std::string &message) {
      std::cout << "[QUIC][" << className << "] " << message << std::endl;
    });

    // Instantiate the HTTP server with hybrid TCP & QUIC transport
    cpphttp::HybridHttpServer server(port);

    // Configure SSL/TLS certificates for HTTPS on the TCP listener
    cpptcpnet::TcpListener::SslConfig ssl_config;
    std::string cert_file = FindCertFile("server.crt");
    std::string key_file = FindCertFile("server.key");

    ssl_config.cert_file = cert_file;
    ssl_config.key_file = key_file;

    try {
      server.GetTransport()->GetTcpTransport().GetListener().EnableSSL(
          ssl_config);
      std::cout << "HTTPS (SSL/TLS) enabled on TCP port." << std::endl;
    } catch (const std::exception &e) {
      std::cerr << "Warning: Failed to enable HTTPS on TCP port: " << e.what()
                << std::endl;
      std::cerr << "Falling back to HTTP (insecure) on TCP port." << std::endl;
    }

    // Configure the same certificates for the QUIC server
    try {
      server.GetTransport()->GetQuicTransport().GetServer().SetCertificate(
          cert_file, key_file);
      std::cout << "Custom SSL/TLS certificates loaded for QUIC server."
                << std::endl;
    } catch (const std::exception &e) {
      std::cerr
          << "Warning: Failed to load custom certificate for QUIC server: "
          << e.what() << std::endl;
      std::cerr
          << "Falling back to auto-generated self-signed certificate for QUIC."
          << std::endl;
    }

    server.Get("/", [](const cpphttp::HttpRequest &req) {
      return cpphttp::HttpResponse::Html(
          "<!DOCTYPE html>\n"
          "<html>\n"
          "<head><title>HTTP-over-QUIC Server</title></head>\n"
          "<body>\n"
          "  <h1>Hello from HTTP-over-QUIC!</h1>\n"
          "  <p>This page is served using the production-ready cpphttp server "
          "over a custom QUIC transport layer.</p>\n"
          "  <ul>\n"
          "    <li><a href=\"/about\">About Page</a></li>\n"
          "    <li><a href=\"/api/stats\">Server Stats API (JSON)</a></li>\n"
          "  </ul>\n"
          "</body>\n"
          "</html>\n");
    });

    server.Get("/about", [](const cpphttp::HttpRequest &req) {
      return cpphttp::HttpResponse::Html(
          "<!DOCTYPE html>\n"
          "<html>\n"
          "<head><title>About HTTP-over-QUIC</title></head>\n"
          "<body>\n"
          "  <h1>About This Server</h1>\n"
          "  <p>This is a C++ implementation of HTTP/1.1 parsed and routed by "
          "cpp-http, running over the custom QUIC library cpp-quic.</p>\n"
          "  <p><a href=\"/\">Back to Home</a></p>\n"
          "</body>\n"
          "</html>\n");
    });

    server.Get("/api/stats", [&server](const cpphttp::HttpRequest &req) {
      auto &quic_server = server.GetTransport()->GetServer();
      auto stats = quic_server.GetStats();
      std::ostringstream json;
      json << "{\n"
           << "  \"active_connections\": " << stats.active_connections << ",\n"
           << "  \"bytes_sent\": " << stats.bytes_sent << ",\n"
           << "  \"bytes_received\": " << stats.bytes_received << ",\n"
           << "  \"packets_sent\": " << stats.packets_sent << ",\n"
           << "  \"packets_received\": " << stats.packets_received << ",\n"
           << "  \"streams_opened\": " << stats.streams_opened << ",\n"
           << "  \"streams_closed\": " << stats.streams_closed << ",\n"
           << "  \"retransmissions\": " << stats.retransmissions << "\n"
           << "}";
      return cpphttp::HttpResponse::Json(json.str(), 200);
    });

    server.Start();

    std::cout << "=================================================="
              << std::endl;
    std::cout
        << "  Hybrid HTTP/QUIC Server Running on TCP (HTTPS) & UDP (QUIC) Port "
        << port << std::endl;
    std::cout << "  Local URL: https://localhost:" << port << "/" << std::endl;
    std::cout << "  Press Enter to stop the server..." << std::endl;
    std::cout << "=================================================="
              << std::endl;
    std::cin.get();
    server.Stop();
  } catch (const std::exception &e) {
    std::cerr << "Failed to start server: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
