#include <fstream>
#include <iostream>
#include <string>

#include "cpphttp.hpp"

int main(int argc, char *argv[]) {
  std::string host = "127.0.0.1";
  uint16_t port = 4433;
  std::string path = "/";
  std::string save_filename = "index.html";

  if (argc > 1) {
    host = argv[1];
  }
  if (argc > 2) {
    try {
      port = static_cast<uint16_t>(std::stoi(argv[2]));
    } catch (...) {
      std::cerr << "Invalid port. Using 4433." << std::endl;
    }
  }
  if (argc > 3) {
    path = argv[3];
  }
  if (argc > 4) {
    save_filename = argv[4];
  }

  std::cout << "[HTTP Client] Connecting to " << host << ":" << port
            << std::endl;
  std::cout << "[HTTP Client] Requesting path: " << path << std::endl;

  try {
    // Enable client-side QUIC logging
    cppquic::SetLogger([](cppquic::LogSeverity severity,
                          const std::string &className,
                          const std::string &message) {
      std::cout << "[CLIENT-QUIC][" << className << "] " << message
                << std::endl;
    });

    cpphttp::HttpClientImpl<cpphttp::QuicClientTransport> client(host, port);
    client.SetTimeout(std::chrono::seconds(5));

    std::unordered_map<std::string, std::string> headers;
    headers["User-Agent"] = "cpp-quic-client/1.0";
    headers["Accept"] = "text/html, application/json";

    cpphttp::HttpResponse response = client.Get(path, headers);

    std::cout << "\n=============================================="
              << std::endl;
    std::cout << "           HTTP RESPONSE HEADERS" << std::endl;
    std::cout << "==============================================" << std::endl;
    // Show headers and metadata
    std::cout << "\033[36m"
              << "HTTP/1.1 " << response.status_code << " "
              << response.status_message << "\n";
    for (const auto &[k, v] : response.headers) {
      std::cout << k << ": " << v << "\n";
    }
    std::cout << "\033[0m" << std::endl;
    std::cout << "==============================================\n"
              << std::endl;

    if (response.status_code == 200) {
      auto ct_it = response.headers.find("Content-Type");
      bool is_json = false;
      if (ct_it != response.headers.end() &&
          ct_it->second.find("application/json") != std::string::npos) {
        is_json = true;
      }

      if (is_json) {
        std::cout << "[HTTP Client] Received JSON response body:\n"
                  << std::endl;
        std::cout << "\033[32m" << response.body << "\033[0m\n" << std::endl;
      } else {
        // Save HTML response
        std::ofstream outfile(save_filename);
        if (outfile.is_open()) {
          outfile << response.body;
          outfile.close();
          std::cout << "[HTTP Client] Successfully saved response body to: "
                    << save_filename << std::endl;
          std::cout << "[HTTP Client] You can open it in a web browser to view "
                       "the page."
                    << std::endl;
        } else {
          std::cerr << "[HTTP Client Error] Failed to open " << save_filename
                    << " for writing." << std::endl;
        }

        // Show brief preview of body
        std::cout << "\n[HTTP Client] Body Preview (first 400 chars):\n"
                  << std::endl;
        std::string preview = response.body.substr(0, 400);
        std::cout << preview << (response.body.size() > 400 ? "..." : "")
                  << std::endl;
      }
    } else {
      std::cout << "[HTTP Client] Server returned non-200 response body:\n"
                << std::endl;
      std::cout << response.body << std::endl;
    }
  } catch (const std::exception &e) {
    std::cerr << "[HTTP Client] Exception: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
