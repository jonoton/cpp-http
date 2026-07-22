#include <chrono>
#include <filesystem>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "cpphttp.hpp"

// Define payload size (20 MB)
const size_t PAYLOAD_SIZE = 20 * 1024 * 1024;

inline double BytesToMB(size_t bytes) {
  return static_cast<double>(bytes) / (1024.0 * 1024.0);
}

inline double CalculateThroughputMBps(size_t bytes, double duration_seconds) {
  if (duration_seconds <= 0.0) return 0.0;
  return BytesToMB(bytes) / duration_seconds;
}

std::string FindCertFile(const std::string &filename) {
  if (std::filesystem::exists(filename)) return filename;
  std::string path = "certs/" + filename;
  if (std::filesystem::exists(path)) return path;
  path = "../certs/" + filename;
  if (std::filesystem::exists(path)) return path;
  path = "../../certs/" + filename;
  if (std::filesystem::exists(path)) return path;
  path = "../../../certs/" + filename;
  if (std::filesystem::exists(path)) return path;
  path = "/workspaces/cpp-http/certs/" + filename;
  if (std::filesystem::exists(path)) return path;
  return "certs/" + filename;
}

int main() {
  uint16_t port = 8080;

  cpphttp::HybridHttpServer server(port);
  server.SetMaxBodySize(PAYLOAD_SIZE * 2);
  server.SetMaxOutboundBufferSize(PAYLOAD_SIZE * 2);
  server.SetSendChunkSize(128 * 1024);

  cpptcpnet::TcpListener::SslConfig ssl_config;
  std::string cert_file = FindCertFile("server.crt");
  std::string key_file = FindCertFile("server.key");
  ssl_config.cert_file = cert_file;
  ssl_config.key_file = key_file;

  cppquic::QuicProfile server_quic_profile =
      cppquic::QuicProfile::HighThroughput();
  server.SetQuicProfile(server_quic_profile);

  try {
    server.GetTransport()->GetTcpTransport().GetListener().EnableSSL(
        ssl_config);
    server.GetTransport()->GetQuicTransport().GetServer().SetCertificate(
        cert_file, key_file);
  } catch (const std::exception &e) {
    std::cerr << "Failed to configure SSL: " << e.what() << std::endl;
    return 1;
  }

  // HTTP GET endpoint to download a large payload
  server.Get("/download", [](const cpphttp::HttpRequest &req) {
    cpphttp::HttpResponse res;
    res.status_code = 200;
    res.status_message = "OK";
    res.headers["Content-Type"] = "application/octet-stream";
    res.body = std::string(PAYLOAD_SIZE, 'a');
    return res;
  });

  // WebSocket endpoint that echoes chunks back
  cpphttp::WebSocketBehavior ws_behavior;
  ws_behavior.on_message =
      [](std::shared_ptr<cpphttp::WebSocketConnection> conn,
         const std::string &msg) { conn->Send(msg); };
  server.WebSocket("/ws", std::move(ws_behavior));

  std::cout << "Starting server on port " << port << "..." << std::endl;
  try {
    server.Start();
  } catch (const std::exception &e) {
    std::cerr << "Failed to start server: " << e.what() << std::endl;
    return 1;
  }

  // Wait for the server to be fully ready
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  std::cout << "Payload size: " << BytesToMB(PAYLOAD_SIZE) << " MB\n"
            << std::endl;

  // 1. HTTP/1.1 (TCP) Throughput
  {
    std::cout << "--------------------------------------------------"
              << std::endl;
    std::cout << " 1. Testing HTTP/1.1 (TCP) Throughput..." << std::endl;
    std::cout << "--------------------------------------------------"
              << std::endl;
    cpphttp::HttpClient tcp_client("127.0.0.1", port);
    tcp_client.SetMaxBodySize(PAYLOAD_SIZE * 2);
    tcp_client.SetMaxOutboundBufferSize(PAYLOAD_SIZE * 2);
    tcp_client.SetSendChunkSize(128 * 1024);
    tcp_client.SetTimeout(std::chrono::seconds(5));
    tcp_client.EnableSSL();

    try {
      auto start = std::chrono::high_resolution_clock::now();
      cpphttp::HttpResponse res = tcp_client.Get("/download");
      auto end = std::chrono::high_resolution_clock::now();

      if (res.status_code == 200 && res.body.size() == PAYLOAD_SIZE) {
        double duration = std::chrono::duration<double>(end - start).count();
        double throughput = CalculateThroughputMBps(PAYLOAD_SIZE, duration);
        std::cout << "  Duration:   " << duration << " s\n";
        std::cout << "  Throughput: " << throughput << " MB/s\n";
      } else {
        std::cerr << "  HTTP/1.1 request failed! Status: " << res.status_code
                  << ", Expected: " << PAYLOAD_SIZE << " bytes ("
                  << BytesToMB(PAYLOAD_SIZE)
                  << " MB), Received: " << res.body.size() << " bytes ("
                  << BytesToMB(res.body.size()) << " MB)" << std::endl;
      }
    } catch (const std::exception &e) {
      std::cerr << "  HTTP/1.1 Exception: " << e.what() << std::endl;
    }
  }

  // 2. HTTP/3 (QUIC) Throughput (Initial Handshake + Warm Connection)
  {
    std::cout << "\n--------------------------------------------------"
              << std::endl;
    std::cout << " 2. Testing HTTP/3 (QUIC) Throughput..." << std::endl;
    std::cout << "--------------------------------------------------"
              << std::endl;
    cpphttp::HttpClientImpl<cpphttp::QuicClientTransport> quic_client(
        "127.0.0.1", port);
    quic_client.SetMaxBodySize(PAYLOAD_SIZE * 2);
    quic_client.SetMaxOutboundBufferSize(PAYLOAD_SIZE * 2);
    quic_client.SetTimeout(std::chrono::seconds(8));
    quic_client.SetSendChunkSize(128 * 1024);
    quic_client.EnableSSL();

    cppquic::QuicProfile client_quic_profile =
        cppquic::QuicProfile::HighThroughput();
    quic_client.SetQuicProfile(client_quic_profile);

    try {
      std::cout << "  Starting QUIC Get(\"/download\")..." << std::flush;
      auto start = std::chrono::high_resolution_clock::now();
      cpphttp::HttpResponse res = quic_client.Get("/download");
      auto end = std::chrono::high_resolution_clock::now();

      if (res.status_code == 200 && res.body.size() == PAYLOAD_SIZE) {
        double duration = std::chrono::duration<double>(end - start).count();
        double throughput = CalculateThroughputMBps(PAYLOAD_SIZE, duration);
        std::cout << " Done! Status: " << res.status_code
                  << ", size: " << res.body.size() << "\n";
        std::cout << "  Initial Handshake Pass:\n";
        std::cout << "    Duration:   " << duration << " s\n";
        std::cout << "    Throughput: " << throughput << " MB/s\n";
      } else {
        std::cerr << "\n  HTTP/3 request failed! Status: " << res.status_code
                  << ", Expected: " << PAYLOAD_SIZE << " bytes ("
                  << BytesToMB(PAYLOAD_SIZE)
                  << " MB), Received: " << res.body.size() << " bytes ("
                  << BytesToMB(res.body.size()) << " MB)" << std::endl;
      }
    } catch (const std::exception &e) {
      std::cerr << "\n  HTTP/3 Exception: " << e.what() << std::endl;
    }

    // Warm Connection Pass (Re-using active QUIC session)
    try {
      std::cout << "  Starting Warm QUIC Get(\"/download\")..." << std::flush;
      auto start = std::chrono::high_resolution_clock::now();
      cpphttp::HttpResponse warm_res = quic_client.Get("/download");
      auto end = std::chrono::high_resolution_clock::now();

      if (warm_res.status_code == 200 && warm_res.body.size() == PAYLOAD_SIZE) {
        double duration = std::chrono::duration<double>(end - start).count();
        double throughput = CalculateThroughputMBps(PAYLOAD_SIZE, duration);
        std::cout << " Done! Status: " << warm_res.status_code
                  << ", size: " << warm_res.body.size() << "\n";
        std::cout << "  Warm Connection Pass:\n";
        std::cout << "    Duration:   " << duration << " s\n";
        std::cout << "    Throughput: " << throughput << " MB/s\n";
      }
    } catch (const std::exception &e) {
      // Warm pass optional, ignore error if stream closed
    }
  }

  // 3. WebSocket Throughput
  {
    std::cout << "\n--------------------------------------------------"
              << std::endl;
    std::cout << " 3. Testing WebSocket Throughput (Echoing)..." << std::endl;
    std::cout << "--------------------------------------------------"
              << std::endl;
    cpphttp::WebSocketClient ws_client;
    ws_client.SetMaxBodySize(PAYLOAD_SIZE * 2);
    ws_client.SetMaxOutboundBufferSize(PAYLOAD_SIZE * 2);

    size_t chunk_size = 256 * 1024;  // 256 KB chunks
    size_t total_to_send = PAYLOAD_SIZE;
    std::atomic<size_t> total_received{0};
    std::string chunk(chunk_size, 'w');

    std::promise<void> done;
    auto future = done.get_future();

    ws_client.OnMessage([&](const std::string &msg) {
      size_t prev = total_received.fetch_add(msg.size());
      if (prev + msg.size() >= total_to_send) {
        try {
          done.set_value();
        } catch (...) {
        }
      } else {
        ws_client.Send(chunk);
      }
    });

    ws_client.OnClose([&](uint16_t status, const std::string &reason) {
      try {
        done.set_value();
      } catch (...) {
      }
    });

    bool connected = false;
    for (int retry = 0; retry < 3; ++retry) {
      if (ws_client.Connect("wss://127.0.0.1:" + std::to_string(port) + "/ws",
                            std::chrono::seconds(5))) {
        connected = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    if (connected) {
      auto start = std::chrono::high_resolution_clock::now();

      // Start the echo pump
      ws_client.Send(chunk);

      if (future.wait_for(std::chrono::seconds(5)) ==
          std::future_status::ready) {
        auto end = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double>(end - start).count();
        size_t total_bytes_transferred = total_received.load() * 2;
        double throughput =
            CalculateThroughputMBps(total_bytes_transferred, duration);
        std::cout << "  Duration:   " << duration << " s\n";
        std::cout << "  Throughput (Send+Recv): " << throughput << " MB/s\n";
      } else {
        std::cerr << "  WebSocket request timed out!" << std::endl;
      }
      ws_client.Disconnect();
    } else {
      std::cerr << "  WebSocket connection failed!" << std::endl;
    }
  }

  server.Stop();
  std::cout << "\nFinished." << std::endl;
  return 0;
}
