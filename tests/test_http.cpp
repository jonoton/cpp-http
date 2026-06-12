#include <gtest/gtest.h>
#include "cpphttp.hpp"
#include "test_config.hpp"
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>

// ==========================================
// Unit Tests
// ==========================================

TEST(HttpUtilityTest, Base64Encode) {
    EXPECT_EQ(cpphttp::base64_encode(""), "");
    EXPECT_EQ(cpphttp::base64_encode("f"), "Zg==");
    EXPECT_EQ(cpphttp::base64_encode("fo"), "Zm8=");
    EXPECT_EQ(cpphttp::base64_encode("foo"), "Zm9v");
    EXPECT_EQ(cpphttp::base64_encode("foob"), "Zm9vYg==");
    EXPECT_EQ(cpphttp::base64_encode("fooba"), "Zm9vYmE=");
    EXPECT_EQ(cpphttp::base64_encode("foobar"), "Zm9vYmFy");
    EXPECT_EQ(cpphttp::base64_encode("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"), 
              "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXpBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWg==");
}

TEST(HttpUtilityTest, ComputeAcceptKey) {
    // Standard test case from RFC 6455
    EXPECT_EQ(cpphttp::ComputeAcceptKey("dGhlIHNhbXBsZSBub25jZQ=="), "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
}

TEST(WsFrameTest, CreateWsFrameUnmasked) {
    std::string text = "Hello";
    std::vector<uint8_t> frame = cpphttp::CreateWsFrame(0x01, text);
    
    // Opcode: 0x81 (FIN=1, Text frame=1)
    // Length: 5 (since length <= 125, byte 1 is just the length)
    // Payload: 'H', 'e', 'l', 'l', 'o'
    ASSERT_EQ(frame.size(), 7);
    EXPECT_EQ(frame[0], 0x81);
    EXPECT_EQ(frame[1], 0x05);
    EXPECT_EQ(frame[2], 'H');
    EXPECT_EQ(frame[3], 'e');
    EXPECT_EQ(frame[4], 'l');
    EXPECT_EQ(frame[5], 'l');
    EXPECT_EQ(frame[6], 'o');
}

TEST(WsFrameTest, ParseWsFrameMaskedSucceeds) {
    // Opcode: 0x81 (FIN=1, Text frame=1)
    // Length byte: 0x85 (Mask bit=1, Length=5)
    // Mask Key: 0x11, 0x22, 0x33, 0x44
    // Payload: "Hello" masked with the key
    // 'H' (0x48) ^ 0x11 = 0x59
    // 'e' (0x65) ^ 0x22 = 0x47
    // 'l' (0x6c) ^ 0x33 = 0x5f
    // 'l' (0x6c) ^ 0x44 = 0x28
    // 'o' (0x6f) ^ 0x11 = 0x7e
    std::vector<uint8_t> buffer = {
        0x81,
        0x85,
        0x11, 0x22, 0x33, 0x44,
        0x59, 0x47, 0x5F, 0x28, 0x7E
    };
    
    cpphttp::WsFrame frame;
    bool protocol_error = false;
    bool parsed = cpphttp::ParseWsFrame(buffer, frame, protocol_error);
    
    EXPECT_TRUE(parsed);
    EXPECT_FALSE(protocol_error);
    EXPECT_TRUE(frame.fin);
    EXPECT_EQ(frame.opcode, 1);
    EXPECT_TRUE(frame.masked);
    EXPECT_EQ(frame.mask_key[0], 0x11);
    EXPECT_EQ(frame.mask_key[1], 0x22);
    EXPECT_EQ(frame.mask_key[2], 0x33);
    EXPECT_EQ(frame.mask_key[3], 0x44);
    
    std::string payload_str(frame.payload.begin(), frame.payload.end());
    EXPECT_EQ(payload_str, "Hello");
    EXPECT_TRUE(buffer.empty()); // The frame data should be consumed from the buffer
}

TEST(WsFrameTest, ParseWsFrameUnmaskedFailsWithProtocolError) {
    // Client-to-server frames MUST be masked. If we parse a frame that isn't masked, it's a protocol error.
    std::vector<uint8_t> buffer = {
        0x81,
        0x05, // Mask bit = 0
        'H', 'e', 'l', 'l', 'o'
    };
    
    cpphttp::WsFrame frame;
    bool protocol_error = false;
    bool parsed = cpphttp::ParseWsFrame(buffer, frame, protocol_error);
    
    EXPECT_FALSE(parsed);
    EXPECT_TRUE(protocol_error);
}

TEST(WsFrameTest, ParseWsFramePartialBuffer) {
    std::vector<uint8_t> buffer = {
        0x81,
        0x85,
        0x11, 0x22, 0x33, 0x44,
        0x59, 0x47 // only 2 bytes of payload, need 5
    };
    
    cpphttp::WsFrame frame;
    bool protocol_error = false;
    bool parsed = cpphttp::ParseWsFrame(buffer, frame, protocol_error);
    
    EXPECT_FALSE(parsed);
    EXPECT_FALSE(protocol_error);
    // Buffer shouldn't be consumed yet
    EXPECT_EQ(buffer.size(), 8);
}

TEST(HttpUtilityTest, UrlDecode) {
    EXPECT_EQ(cpphttp::url_decode(""), "");
    EXPECT_EQ(cpphttp::url_decode("hello%20world"), "hello world");
    EXPECT_EQ(cpphttp::url_decode("hello+world"), "hello world");
    EXPECT_EQ(cpphttp::url_decode("%3Chello%3E"), "<hello>");
}

TEST(HttpRoutingTest, MatchRoute) {
    std::unordered_map<std::string, std::string> params;
    
    // Exact match
    EXPECT_TRUE(cpphttp::match_route("GET", {"api", "users"}, "GET", {"api", "users"}, params));
    EXPECT_TRUE(params.empty());
    
    // Method mismatch
    EXPECT_FALSE(cpphttp::match_route("POST", {"api", "users"}, "GET", {"api", "users"}, params));
    
    // Parameter match
    EXPECT_TRUE(cpphttp::match_route("GET", {"users", "123"}, "GET", {"users", ":id"}, params));
    EXPECT_EQ(params["id"], "123");
    
    // Wildcard match
    EXPECT_TRUE(cpphttp::match_route("GET", {"static", "css", "style.css"}, "GET", {"static", "*"}, params));
    EXPECT_EQ(params["*"], "css/style.css");
    
    // Wildcard empty match
    EXPECT_TRUE(cpphttp::match_route("GET", {"static"}, "GET", {"static", "*"}, params));
    EXPECT_EQ(params["*"], "");
}

TEST(HttpRoutingTest, PathTraversalMitigation) {
    // Check that split_path resolves '.' and '..'
    std::vector<std::string> segs = cpphttp::split_path("/static/../etc/passwd");
    ASSERT_EQ(segs.size(), 2);
    EXPECT_EQ(segs[0], "etc");
    EXPECT_EQ(segs[1], "passwd");

    std::vector<std::string> segs2 = cpphttp::split_path("/a/./b/../c");
    ASSERT_EQ(segs2.size(), 2);
    EXPECT_EQ(segs2[0], "a");
    EXPECT_EQ(segs2[1], "c");

    std::vector<std::string> segs3 = cpphttp::split_path("/../../etc/passwd");
    ASSERT_EQ(segs3.size(), 2);
    EXPECT_EQ(segs3[0], "etc");
    EXPECT_EQ(segs3[1], "passwd");
}

TEST(HttpResponseBuilderTest, ResponseHelpers) {
    auto json_res = cpphttp::HttpResponse::Json("{\"status\":\"ok\"}", 201);
    EXPECT_EQ(json_res.status_code, 201);
    EXPECT_EQ(json_res.status_message, "Created");
    EXPECT_EQ(json_res.headers.at("Content-Type"), "application/json");
    EXPECT_EQ(json_res.body, "{\"status\":\"ok\"}");

    auto redirect_res = cpphttp::HttpResponse::Redirect("https://google.com", 301);
    EXPECT_EQ(redirect_res.status_code, 301);
    EXPECT_EQ(redirect_res.status_message, "Moved Permanently");
    EXPECT_EQ(redirect_res.headers.at("Location"), "https://google.com");
}

TEST(HttpIntegrationTest, CompleteServerAndClientFlow) {
    cpphttp::HttpServer server(8091);

    // Register a middleware to inject a custom request header
    server.Use([](cpphttp::HttpRequest &req, cpphttp::HttpResponse &res) {
        req.headers["X-Middleware-Seen"] = "Yes";
        if (req.path == "/unauthorized") {
            res = cpphttp::HttpResponse::Plain("Unauthorized", 401);
            return false; // abort chain
        }
        return true;
    });

    // Simple path parameter route
    server.Get("/users/:id/profile", [](const cpphttp::HttpRequest &req) {
        std::string id = req.path_params.at("id");
        std::string param_val = req.query_params.count("token") ? req.query_params.at("token") : "none";
        std::string mid_seen = req.headers.count("X-Middleware-Seen") ? req.headers.at("X-Middleware-Seen") : "No";
        return cpphttp::HttpResponse::Json("{\"id\":\"" + id + "\",\"token\":\"" + param_val + "\",\"middleware\":\"" + mid_seen + "\"}");
    });

    // Dynamic wildcard route
    server.Get("/assets/*", [](const cpphttp::HttpRequest &req) {
        std::string filepath = req.path_params.at("*");
        return cpphttp::HttpResponse::Plain("Serving: " + filepath);
    });

    // Post route
    server.Post("/submit", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("Received body: " + req.body, 201);
    });

    server.Start();

    // Setup HttpClient
    cpphttp::HttpClient client("127.0.0.1", 8091);

    // Test GET with path params and query parameters
    try {
        auto res = client.Get("/users/456/profile?token=secret123");
        EXPECT_EQ(res.status_code, 200);
        EXPECT_EQ(res.headers.at("Content-Type"), "application/json");
        EXPECT_NE(res.body.find("\"id\":\"456\""), std::string::npos);
        EXPECT_NE(res.body.find("\"token\":\"secret123\""), std::string::npos);
        EXPECT_NE(res.body.find("\"middleware\":\"Yes\""), std::string::npos);
    } catch (const std::exception &e) {
        FAIL() << "GET Request failed: " << e.what();
    }

    // Test wildcard GET
    try {
        auto res = client.Get("/assets/js/libs/jquery.min.js");
        EXPECT_EQ(res.status_code, 200);
        EXPECT_EQ(res.body, "Serving: js/libs/jquery.min.js");
    } catch (const std::exception &e) {
        FAIL() << "Wildcard GET Request failed: " << e.what();
    }

    // Test POST with body
    try {
        auto res = client.Post("/submit", "Hello C++ HTTP Server");
        EXPECT_EQ(res.status_code, 201);
        EXPECT_EQ(res.body, "Received body: Hello C++ HTTP Server");
    } catch (const std::exception &e) {
        FAIL() << "POST Request failed: " << e.what();
    }

    // Test Middleware Abort / Unauthorized route
    try {
        auto res = client.Get("/unauthorized");
        EXPECT_EQ(res.status_code, 401);
        EXPECT_EQ(res.body, "Unauthorized");
    } catch (const std::exception &e) {
        FAIL() << "Middleware Abort Request failed: " << e.what();
    }

    // Test 404 Route
    try {
        auto res = client.Get("/nonexistent");
        EXPECT_EQ(res.status_code, 404);
    } catch (const std::exception &e) {
        FAIL() << "404 Route Request failed: " << e.what();
    }

    server.Stop();
}

TEST(HttpConfigTest, GetSetSettings) {
    cpphttp::HttpServer server(8092);
    EXPECT_EQ(server.GetMaxHeaderSize(), 8192);
    server.SetMaxHeaderSize(4096);
    EXPECT_EQ(server.GetMaxHeaderSize(), 4096);
    
    cpphttp::HttpClient client("127.0.0.1", 8092);
    EXPECT_EQ(client.GetTimeout().count(), 10000);
    client.SetTimeout(std::chrono::milliseconds(2000));
    EXPECT_EQ(client.GetTimeout().count(), 2000);
}

// ==========================================
// Integration Tests Infrastructure
// ==========================================

struct TestClientSession {
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<uint8_t> buffer;
    bool connected = false;
    bool disconnected = false;
};

bool PerformWsHandshake(std::shared_ptr<TestClientSession> session, cpptcpnet::TcpClient &client, uint64_t session_id, const std::string &path) {
    std::string key = "dGhlIHNhbXBsZSBub25jZQ=="; // standard key
    std::string handshake_req =
        "GET " + path + " HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: " + key + "\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n";
    
    // Clear buffer before sending
    {
        std::lock_guard<std::mutex> lock(session->mutex);
        session->buffer.clear();
    }
    
    // Send handshake request
    if (!client.Send(session_id, handshake_req)) {
        return false;
    }
    
    // Wait for full HTTP response headers (terminator "\r\n\r\n")
    std::unique_lock<std::mutex> lock(session->mutex);
    auto check_handshake = [&]() {
        std::string buffer_str(session->buffer.begin(), session->buffer.end());
        return buffer_str.find("\r\n\r\n") != std::string::npos;
    };
    
    if (!session->cv.wait_for(lock, std::chrono::seconds(5), check_handshake)) {
        return false; // timeout
    }
    
    std::string response(session->buffer.begin(), session->buffer.end());
    // Verify 101 Switching Protocols and Sec-WebSocket-Accept
    if (response.find("101 Switching Protocols") == std::string::npos) {
        return false;
    }
    std::string expected_accept = "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";
    if (response.find(expected_accept) == std::string::npos) {
        return false;
    }
    
    // Consume the handshake response from the buffer
    size_t terminator_pos = response.find("\r\n\r\n");
    session->buffer.erase(session->buffer.begin(), session->buffer.begin() + terminator_pos + 4);
    
    return true;
}

bool SendWsTextFrame(cpptcpnet::TcpClient &client, uint64_t session_id, const std::string &text) {
    std::vector<uint8_t> payload(text.begin(), text.end());
    std::vector<uint8_t> frame;
    
    frame.push_back(0x81); // FIN=1, opcode=1 (text)
    frame.push_back(0x80 | payload.size()); // Mask bit=1, length
    
    // Use a simple mask key: 0x11, 0x22, 0x33, 0x44
    uint8_t mask_key[4] = {0x11, 0x22, 0x33, 0x44};
    for (int i = 0; i < 4; ++i) {
        frame.push_back(mask_key[i]);
    }
    
    for (size_t i = 0; i < payload.size(); ++i) {
        frame.push_back(payload[i] ^ mask_key[i % 4]);
    }
    
    return client.Send(session_id, frame);
}

bool ExpectWsTextFrame(std::shared_ptr<TestClientSession> session, std::string &out_text) {
    std::unique_lock<std::mutex> lock(session->mutex);
    
    auto has_enough_data = [&]() {
        if (session->buffer.size() < 2) return false;
        uint8_t len_byte = session->buffer[1];
        bool masked = (len_byte & 0x80) != 0;
        uint64_t payload_len = len_byte & 0x7F;
        size_t expected_size = 2 + (masked ? 4 : 0) + payload_len;
        return session->buffer.size() >= expected_size;
    };
    
    if (!session->cv.wait_for(lock, std::chrono::seconds(5), has_enough_data)) {
        return false; // timeout
    }
    
    // Parse the frame
    uint8_t opcode = session->buffer[0] & 0x0F;
    if (opcode != 0x01) {
        return false; // not text frame
    }
    
    uint8_t len_byte = session->buffer[1];
    bool masked = (len_byte & 0x80) != 0;
    if (masked) {
        return false; // Server-to-client frames MUST NOT be masked
    }
    
    uint64_t payload_len = len_byte & 0x7F;
    out_text = std::string(session->buffer.begin() + 2, session->buffer.begin() + 2 + payload_len);
    
    // Consume frame
    session->buffer.erase(session->buffer.begin(), session->buffer.begin() + 2 + payload_len);
    return true;
}

bool SendWsCloseFrame(cpptcpnet::TcpClient &client, uint64_t session_id) {
    std::vector<uint8_t> frame = {
        0x88, // FIN=1, opcode=8 (close)
        0x82, // Mask bit=1, length=2
        0x11, 0x22, 0x33, 0x44, // Mask key
        static_cast<uint8_t>((1000 >> 8) ^ 0x11), // Status code 1000 masked
        static_cast<uint8_t>((1000 & 0xFF) ^ 0x22)
    };
    return client.Send(session_id, frame);
}

bool ExpectWsCloseFrame(std::shared_ptr<TestClientSession> session) {
    std::unique_lock<std::mutex> lock(session->mutex);
    
    auto has_close_frame = [&]() {
        if (session->buffer.size() < 2) return false;
        uint8_t len_byte = session->buffer[1];
        bool masked = (len_byte & 0x80) != 0;
        uint64_t payload_len = len_byte & 0x7F;
        size_t expected_size = 2 + (masked ? 4 : 0) + payload_len;
        return session->buffer.size() >= expected_size;
    };
    
    if (!session->cv.wait_for(lock, std::chrono::seconds(5), has_close_frame)) {
        return false; // timeout
    }
    
    uint8_t opcode = session->buffer[0] & 0x0F;
    if (opcode != 0x08) {
        return false; // not close frame
    }
    
    uint8_t len_byte = session->buffer[1];
    uint64_t payload_len = len_byte & 0x7F;
    session->buffer.erase(session->buffer.begin(), session->buffer.begin() + 2 + payload_len);
    return true;
}

// ==========================================
// Integration Tests
// ==========================================

TEST(WebSocketIntegrationTest, PlainEcho) {
    cpphttp::HttpServer server(8089);
    
    cpphttp::WebSocketBehavior ws_behavior;
    ws_behavior.on_open = [](std::shared_ptr<cpphttp::WebSocketConnection> conn) {};
    ws_behavior.on_message = [](std::shared_ptr<cpphttp::WebSocketConnection> conn, const std::string &msg) {
        conn->Send("Server Echo: " + msg);
    };
    ws_behavior.on_close = [](std::shared_ptr<cpphttp::WebSocketConnection> conn) {};
    server.WebSocket("/ws", std::move(ws_behavior));
    
    EXPECT_NO_THROW(server.Start());
    
    // Setup Client
    cpptcpnet::TcpClient client;
    auto session = std::make_shared<TestClientSession>();
    
    client.SetDataHandler([session](uint64_t session_id, const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(session->mutex);
        session->buffer.insert(session->buffer.end(), data.begin(), data.end());
        session->cv.notify_all();
    });
    
    cpppubsub::Worker worker;
    auto sub = client.GetEventBroker().Subscribe<cpptcpnet::ConnectionEvent>("state_events");
    worker.AddSubscription<cpptcpnet::ConnectionEvent>(sub, [&](const cpptcpnet::ConnectionEvent& e) {
        std::lock_guard<std::mutex> lock(session->mutex);
        if (e.state == cpptcpnet::ConnectionState::Connected) {
            session->connected = true;
        } else {
            session->disconnected = true;
        }
        session->cv.notify_all();
    });
    worker.Start();
    
    uint64_t session_id = client.Connect("127.0.0.1", 8089);
    ASSERT_GT(session_id, 0);
    
    // Wait for connection to establish
    {
        std::unique_lock<std::mutex> lock(session->mutex);
        session->cv.wait_for(lock, std::chrono::seconds(5), [&]() { return session->connected; });
        ASSERT_TRUE(session->connected);
    }
    
    // Perform Handshake
    ASSERT_TRUE(PerformWsHandshake(session, client, session_id, "/ws"));
    
    // Send Text message
    ASSERT_TRUE(SendWsTextFrame(client, session_id, "Hello, WS!"));
    
    // Expect Echo response
    std::string response_text;
    ASSERT_TRUE(ExpectWsTextFrame(session, response_text));
    EXPECT_EQ(response_text, "Server Echo: Hello, WS!");
    
    // Close WebSocket connection
    ASSERT_TRUE(SendWsCloseFrame(client, session_id));
    ASSERT_TRUE(ExpectWsCloseFrame(session));
    
    // Wait for server to disconnect the socket
    {
        std::unique_lock<std::mutex> lock(session->mutex);
        bool disconnected = session->cv.wait_for(lock, std::chrono::seconds(5), [&]() { return session->disconnected; });
        EXPECT_TRUE(disconnected);
    }
    
    // Cleanup
    client.Stop();
    worker.Stop();
    server.Stop();
}


#if defined(CPPTCPNET_SSL_SUPPORT)
TEST(WebSocketIntegrationTest, SecureEcho) {
    cpphttp::HttpServer server(8090);
    
    // Configure SSL certificates from certs/ directory
    cpptcpnet::TcpListener::SslConfig ssl_config;
    ssl_config.cert_file = CPPHTTP_CERTS_DIR "/server.crt";
    ssl_config.key_file = CPPHTTP_CERTS_DIR "/server.key";
    
    // Verify files exist
    {
        std::ifstream cert_file(ssl_config.cert_file);
        std::ifstream key_file(ssl_config.key_file);
        ASSERT_TRUE(cert_file.good()) << "Certificate file not found: " << ssl_config.cert_file;
        ASSERT_TRUE(key_file.good()) << "Key file not found: " << ssl_config.key_file;
    }
    
    ASSERT_NO_THROW(server.GetListener().EnableSSL(ssl_config));
    
    cpphttp::WebSocketBehavior ws_behavior;
    ws_behavior.on_open = [](std::shared_ptr<cpphttp::WebSocketConnection> conn) {};
    ws_behavior.on_message = [](std::shared_ptr<cpphttp::WebSocketConnection> conn, const std::string &msg) {
        conn->Send("Server Echo: " + msg);
    };
    ws_behavior.on_close = [](std::shared_ptr<cpphttp::WebSocketConnection> conn) {};
    server.WebSocket("/ws", std::move(ws_behavior));
    
    EXPECT_NO_THROW(server.Start());
    
    // Setup Client
    cpptcpnet::TcpClient client;
    ASSERT_NO_THROW(client.EnableSSL()); // defaults to verify_peer = false
    
    auto session = std::make_shared<TestClientSession>();
    
    client.SetDataHandler([session](uint64_t session_id, const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(session->mutex);
        session->buffer.insert(session->buffer.end(), data.begin(), data.end());
        session->cv.notify_all();
    });
    
    cpppubsub::Worker worker;
    auto sub = client.GetEventBroker().Subscribe<cpptcpnet::ConnectionEvent>("state_events");
    worker.AddSubscription<cpptcpnet::ConnectionEvent>(sub, [&](const cpptcpnet::ConnectionEvent& e) {
        std::lock_guard<std::mutex> lock(session->mutex);
        if (e.state == cpptcpnet::ConnectionState::Connected) {
            session->connected = true;
        } else {
            session->disconnected = true;
        }
        session->cv.notify_all();
    });
    worker.Start();
    
    uint64_t session_id = client.Connect("127.0.0.1", 8090);
    ASSERT_GT(session_id, 0);
    
    // Wait for connection to establish over SSL
    {
        std::unique_lock<std::mutex> lock(session->mutex);
        session->cv.wait_for(lock, std::chrono::seconds(5), [&]() { return session->connected; });
        ASSERT_TRUE(session->connected);
    }
    
    // Perform Handshake
    ASSERT_TRUE(PerformWsHandshake(session, client, session_id, "/ws"));
    
    // Send Text message
    ASSERT_TRUE(SendWsTextFrame(client, session_id, "Hello, WSS!"));
    
    // Expect Echo response
    std::string response_text;
    ASSERT_TRUE(ExpectWsTextFrame(session, response_text));
    EXPECT_EQ(response_text, "Server Echo: Hello, WSS!");
    
    // Close WebSocket connection
    ASSERT_TRUE(SendWsCloseFrame(client, session_id));
    ASSERT_TRUE(ExpectWsCloseFrame(session));
    
    // Wait for server to disconnect the socket
    {
        std::unique_lock<std::mutex> lock(session->mutex);
        bool disconnected = session->cv.wait_for(lock, std::chrono::seconds(5), [&]() { return session->disconnected; });
        EXPECT_TRUE(disconnected);
    }
    
    // Cleanup
    client.Stop();
    worker.Stop();
    server.Stop();
}
#endif

// ==========================================
// New Tests for Bug Fixes & Async Client Calls
// ==========================================

TEST(HttpIntegrationTest, CaseInsensitiveHeadersAndChunkedEncoding) {
    cpphttp::HttpServer server(8101);
    
    server.Post("/test", [](const cpphttp::HttpRequest &req) {
        std::string x_test = req.GetHeader("X-TEST-HEADER");
        return cpphttp::HttpResponse::Plain("X-Test: " + x_test + ", Body: " + req.body);
    });

    server.Start();

    cpphttp::HttpClient client("127.0.0.1", 8101);
    
    try {
        std::unordered_map<std::string, std::string> headers = {
            {"x-teSt-hEaDeR", "value123"},
            {"Transfer-Encoding", "chunked"}
        };
        std::string chunked_body = "5\r\nHello\r\n6\r\n World\r\n0\r\n\r\n";
        
        auto res = client.SendRequest("POST", "/test", chunked_body, headers);
        EXPECT_EQ(res.status_code, 200);
        EXPECT_EQ(res.body, "X-Test: value123, Body: Hello World");
    } catch (const std::exception &e) {
        FAIL() << "Request failed: " << e.what();
    }

    server.Stop();
}

TEST(HttpIntegrationTest, PipelinedRequestsAndEofBody) {
    cpphttp::HttpServer server(8102);

    server.Get("/r1", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("Response 1");
    });
    server.Get("/r2", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("Response 2");
    });

    server.Start();

    cpptcpnet::TcpClient tcp_client;
    std::mutex mtx;
    std::condition_variable cv;
    std::vector<uint8_t> buffer;
    bool disconnected = false;

    tcp_client.SetDataHandler([&](uint64_t /*session_id*/, const std::vector<uint8_t> &data) {
        std::lock_guard<std::mutex> lock(mtx);
        buffer.insert(buffer.end(), data.begin(), data.end());
        cv.notify_all();
    });

    auto sub = tcp_client.GetEventBroker().Subscribe<cpptcpnet::ConnectionEvent>("state_events");
    cpppubsub::Worker worker;
    worker.AddSubscription<cpptcpnet::ConnectionEvent>(sub, [&](const cpptcpnet::ConnectionEvent &e) {
        std::lock_guard<std::mutex> lock(mtx);
        if (e.state == cpptcpnet::ConnectionState::Disconnected) {
            disconnected = true;
            cv.notify_all();
        }
    });
    worker.Start();
    tcp_client.Start();

    uint64_t session_id = tcp_client.Connect("127.0.0.1", 8102);
    ASSERT_GT(session_id, 0);

    std::string pipelined_reqs =
        "GET /r1 HTTP/1.1\r\nHost: 127.0.0.1:8102\r\nConnection: keep-alive\r\n\r\n"
        "GET /r2 HTTP/1.1\r\nHost: 127.0.0.1:8102\r\nConnection: close\r\n\r\n";

    ASSERT_TRUE(tcp_client.Send(session_id, pipelined_reqs));

    std::unique_lock<std::mutex> lock(mtx);
    auto check_pipelined = [&]() {
        std::string res_str(buffer.begin(), buffer.end());
        return res_str.find("Response 1") != std::string::npos &&
               res_str.find("Response 2") != std::string::npos;
    };
    EXPECT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), check_pipelined));

    tcp_client.Stop();
    worker.Stop();
    server.Stop();
}

TEST(HttpIntegrationTest, ClientAsynchronousRequests) {
    cpphttp::HttpServer server(8103);

    server.Get("/async-test", [](const cpphttp::HttpRequest &req) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return cpphttp::HttpResponse::Plain("Async OK");
    });

    server.Start();

    cpphttp::HttpClient client("127.0.0.1", 8103);
    
    auto future = client.GetAsync("/async-test");
    
    EXPECT_TRUE(future.valid());
    
    try {
        auto res = future.get();
        EXPECT_EQ(res.status_code, 200);
        EXPECT_EQ(res.body, "Async OK");
    } catch (const std::exception &e) {
        FAIL() << "Async request failed: " << e.what();
    }

    server.Stop();
}

TEST(HttpIntegrationTest, PayloadSizeLimits) {
    cpphttp::HttpServer server(8104);
    server.SetMaxBodySize(100);

    server.Post("/limit-test", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("OK");
    });

    server.Start();

    cpphttp::HttpClient client("127.0.0.1", 8104);

    try {
        auto res = client.Post("/limit-test", "Small body");
        EXPECT_EQ(res.status_code, 200);
    } catch (const std::exception &e) {
        FAIL() << "Small body failed: " << e.what();
    }

    try {
        std::string large_body(200, 'A');
        auto res = client.Post("/limit-test", large_body);
        EXPECT_EQ(res.status_code, 413);
    } catch (const std::exception &e) {
    }

    server.Stop();
}

TEST(HttpIntegrationTest, ScopedMiddleware) {
    cpphttp::HttpServer server(8105);

    server.Use("/api", [](cpphttp::HttpRequest &req, cpphttp::HttpResponse &res) {
        req.headers["X-API-Middleware"] = "API-Scoped";
        return true;
    });

    server.Use("/web", [](cpphttp::HttpRequest &req, cpphttp::HttpResponse &res) {
        res = cpphttp::HttpResponse::Plain("Blocked by Web Middleware", 403);
        return false;
    });

    server.Get("/api/test", [](const cpphttp::HttpRequest &req) {
        std::string mw = req.headers.count("X-API-Middleware") ? req.headers.at("X-API-Middleware") : "None";
        return cpphttp::HttpResponse::Plain("API: " + mw);
    });

    server.Get("/web/test", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("Web OK");
    });

    server.Get("/other/test", [](const cpphttp::HttpRequest &req) {
        std::string mw = req.headers.count("X-API-Middleware") ? req.headers.at("X-API-Middleware") : "None";
        return cpphttp::HttpResponse::Plain("Other: " + mw);
    });

    server.Start();

    cpphttp::HttpClient client("127.0.0.1", 8105);

    // Test Scoped Middleware executes
    auto res1 = client.Get("/api/test");
    EXPECT_EQ(res1.status_code, 200);
    EXPECT_EQ(res1.body, "API: API-Scoped");

    // Test Scoped Middleware blocks
    auto res2 = client.Get("/web/test");
    EXPECT_EQ(res2.status_code, 403);
    EXPECT_EQ(res2.body, "Blocked by Web Middleware");

    // Test Scoped Middleware doesn't run on other paths
    auto res3 = client.Get("/other/test");
    EXPECT_EQ(res3.status_code, 200);
    EXPECT_EQ(res3.body, "Other: None");

    server.Stop();
}

TEST(HttpMultipartTest, ParseMultipartContent) {
    std::string boundary = "XYZ123";
    std::string body = 
        "--XYZ123\r\n"
        "Content-Disposition: form-data; name=\"username\"\r\n\r\n"
        "john_doe\r\n"
        "--XYZ123\r\n"
        "Content-Disposition: form-data; name=\"profile_pic\"; filename=\"pic.png\"\r\n"
        "Content-Type: image/png\r\n\r\n"
        "PNGDATA\r\n"
        "--XYZ123--\r\n";

    auto parts = cpphttp::ParseMultipart(body, boundary);
    ASSERT_EQ(parts.size(), 2);
    EXPECT_EQ(parts[0].name, "username");
    EXPECT_EQ(parts[0].filename, "");
    EXPECT_EQ(parts[0].data, "john_doe");

    EXPECT_EQ(parts[1].name, "profile_pic");
    EXPECT_EQ(parts[1].filename, "pic.png");
    EXPECT_EQ(parts[1].content_type, "image/png");
    EXPECT_EQ(parts[1].data, "PNGDATA");
}

TEST(HttpCompressionTest, GzipServerAndClient) {
    cpphttp::HttpServer server(8106);

    server.Get("/compress", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("This is a highly compressible message!!! This is a highly compressible message!!!");
    });

    server.Start();

    // Client transparently decompresses
    cpphttp::HttpClient client("127.0.0.1", 8106);
    auto res = client.Get("/compress");
    EXPECT_EQ(res.status_code, 200);
    EXPECT_EQ(res.body, "This is a highly compressible message!!! This is a highly compressible message!!!");

    // Verify it was actually compressed (Content-Encoding header present)
    EXPECT_EQ(res.headers.at("Content-Encoding"), "gzip");

    server.Stop();
}

TEST(HttpSplittingTest, ValidationRejectsCRLF) {
    cpphttp::HttpResponse response;
    response.status_code = 302;
    // Inject CRLF to perform Response Splitting
    response.headers["Location"] = "http://google.com\r\nInjected-Header: evil";
    EXPECT_THROW(response.Serialize(), std::invalid_argument);

    cpphttp::HttpClient client("127.0.0.1", 8080);
    // Path containing CRLF should throw
    EXPECT_THROW(client.Get("/path\r\nEvil-Header: value"), std::invalid_argument);
    // Header value containing CRLF should throw
    EXPECT_THROW(client.Get("/path", {{"X-Header", "value\r\nInjected: evil"}}), std::invalid_argument);
}

TEST(HttpIntegrationTest, KeepAliveAndRedirectFollowing) {
    cpphttp::HttpServer server(8107);

    server.Get("/redirect", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Redirect("/target", 302);
    });

    server.Get("/target", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("Target Reached");
    });

    server.Start();

    cpphttp::HttpClient client("127.0.0.1", 8107);
    
    // Test direct redirect following
    auto res1 = client.Get("/redirect");
    EXPECT_EQ(res1.status_code, 200);
    EXPECT_EQ(res1.body, "Target Reached");

    // Test Keep-Alive reuse: execute second request on same client
    auto res2 = client.Get("/target");
    EXPECT_EQ(res2.status_code, 200);
    EXPECT_EQ(res2.body, "Target Reached");

    server.Stop();
}

TEST(HttpUtilityTest, CaseInsensitiveHeadersAndMultiHeaders) {
    cpphttp::HeaderMap headers;
    headers["Content-Type"] = "text/html";
    headers["x-custom-header"] = "value1";

    EXPECT_EQ(headers.count("content-type"), 1);
    EXPECT_EQ(headers.at("CONTENT-TYPE"), "text/html");
    EXPECT_EQ(headers.at("x-custom-header"), "value1");

    cpphttp::HttpResponse response;
    response.headers["Content-Type"] = "application/json";
    response.multi_headers.insert({"Set-Cookie", "cookie1=val1"});
    response.multi_headers.insert({"set-cookie", "cookie2=val2"});

    std::string serialized = response.Serialize();
    EXPECT_NE(serialized.find("Set-Cookie: cookie1=val1"), std::string::npos);
    EXPECT_NE(serialized.find("set-cookie: cookie2=val2"), std::string::npos);
    EXPECT_NE(serialized.find("Content-Type: application/json"), std::string::npos);
}

TEST(HttpIntegrationTest, MiddlewareResponseDecorationAndMerging) {
    cpphttp::HttpServer server(8108);

    server.Use([](cpphttp::HttpRequest &req, cpphttp::HttpResponse &res) {
        res.headers["X-Middleware-Added"] = "GlobalValue";
        res.multi_headers.insert({"Set-Cookie", "mw_cookie=1"});
        return true;
    });

    server.Get("/test", [](const cpphttp::HttpRequest &req) {
        cpphttp::HttpResponse response = cpphttp::HttpResponse::Plain("Hello");
        response.headers["X-Route-Added"] = "RouteValue";
        return response;
    });

    server.Start();

    cpphttp::HttpClient client("127.0.0.1", 8108);
    auto res = client.Get("/test");
    EXPECT_EQ(res.status_code, 200);
    EXPECT_EQ(res.headers.at("X-Route-Added"), "RouteValue");
    EXPECT_EQ(res.headers.at("X-Middleware-Added"), "GlobalValue");
    EXPECT_EQ(res.multi_headers.find("Set-Cookie")->second, "mw_cookie=1");

    server.Stop();
}

TEST(HttpIntegrationTest, RedirectMethodDowngrade) {
    cpphttp::HttpServer server(8109);

    server.Post("/redirect", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Redirect("/target", 302);
    });

    server.Get("/target", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("GET Target Reached");
    });

    server.Post("/target", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("POST Target Reached");
    });

    server.Start();

    cpphttp::HttpClient client("127.0.0.1", 8109);
    auto res = client.Post("/redirect", "post_body");
    EXPECT_EQ(res.status_code, 200);
    EXPECT_EQ(res.body, "GET Target Reached");

    server.Stop();
}

TEST(WebSocketIntegrationTest, WebSocketClientPlainEcho) {
    cpphttp::HttpServer server(8110);
    
    cpphttp::WebSocketBehavior ws_behavior;
    ws_behavior.on_open = [](std::shared_ptr<cpphttp::WebSocketConnection> conn) {};
    ws_behavior.on_message = [](std::shared_ptr<cpphttp::WebSocketConnection> conn, const std::string &msg) {
        conn->Send("Echo: " + msg);
    };
    server.WebSocket("/ws", std::move(ws_behavior));
    
    server.Start();

    cpphttp::WebSocketClient client;
    std::mutex ws_mtx;
    std::condition_variable ws_cv;
    std::string received_msg;
    bool opened = false;
    bool closed = false;

    client.OnOpen([&]() {
        std::lock_guard<std::mutex> lock(ws_mtx);
        opened = true;
        ws_cv.notify_all();
    });

    client.OnMessage([&](const std::string &msg) {
        std::lock_guard<std::mutex> lock(ws_mtx);
        received_msg = msg;
        ws_cv.notify_all();
    });

    client.OnClose([&](uint16_t code, const std::string &reason) {
        std::lock_guard<std::mutex> lock(ws_mtx);
        closed = true;
        ws_cv.notify_all();
    });

    ASSERT_TRUE(client.Connect("ws://127.0.0.1:8110/ws"));

    {
        std::unique_lock<std::mutex> lock(ws_mtx);
        ws_cv.wait_for(lock, std::chrono::seconds(3), [&]() { return opened; });
        EXPECT_TRUE(opened);
    }

    client.Send("Hello WebSocket");

    {
        std::unique_lock<std::mutex> lock(ws_mtx);
        ws_cv.wait_for(lock, std::chrono::seconds(3), [&]() { return !received_msg.empty(); });
        EXPECT_EQ(received_msg, "Echo: Hello WebSocket");
    }

    client.Close();
    server.Stop();
}

TEST(WebSocketIntegrationTest, InvalidOpcodeFails) {
    cpphttp::HttpServer server(8111);
    
    cpphttp::WebSocketBehavior ws_behavior;
    ws_behavior.on_open = [](std::shared_ptr<cpphttp::WebSocketConnection> conn) {};
    ws_behavior.on_message = [](std::shared_ptr<cpphttp::WebSocketConnection> conn, const std::string &msg) {};
    ws_behavior.on_close = [](std::shared_ptr<cpphttp::WebSocketConnection> conn) {};
    server.WebSocket("/ws", std::move(ws_behavior));
    
    server.Start();
    
    cpptcpnet::TcpClient client;
    auto session = std::make_shared<TestClientSession>();
    
    client.SetDataHandler([session](uint64_t session_id, const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(session->mutex);
        session->buffer.insert(session->buffer.end(), data.begin(), data.end());
        session->cv.notify_all();
    });
    
    cpppubsub::Worker worker;
    auto sub = client.GetEventBroker().Subscribe<cpptcpnet::ConnectionEvent>("state_events");
    worker.AddSubscription<cpptcpnet::ConnectionEvent>(sub, [&](const cpptcpnet::ConnectionEvent& e) {
        std::lock_guard<std::mutex> lock(session->mutex);
        if (e.state == cpptcpnet::ConnectionState::Connected) {
            session->connected = true;
        } else {
            session->disconnected = true;
        }
        session->cv.notify_all();
    });
    worker.Start();
    client.Start();
    
    uint64_t session_id = client.Connect("127.0.0.1", 8111);
    ASSERT_GT(session_id, 0);
    
    {
        std::unique_lock<std::mutex> lock(session->mutex);
        session->cv.wait_for(lock, std::chrono::seconds(5), [&]() { return session->connected; });
        ASSERT_TRUE(session->connected);
    }
    
    ASSERT_TRUE(PerformWsHandshake(session, client, session_id, "/ws"));
    
    // Send frame with invalid opcode 0x03
    std::vector<uint8_t> invalid_frame = {
        0x83, // FIN=1, opcode=3 (Reserved)
        0x80, // Mask bit=1, length=0
        0x11, 0x22, 0x33, 0x44 // Mask key
    };
    ASSERT_TRUE(client.Send(session_id, invalid_frame));
    
    // Expect Server to close connection with Protocol Error (1002)
    ASSERT_TRUE(ExpectWsCloseFrame(session));
    
    client.Stop();
    worker.Stop();
    server.Stop();
}

TEST(HttpUtilityTest, SecureWebSocketUrlDefaultPort) {
    std::string protocol, host, path;
    uint16_t port = 0;
    EXPECT_TRUE(cpphttp::parse_url("wss://example.com/ws", protocol, host, port, path));
    EXPECT_EQ(protocol, "wss");
    EXPECT_EQ(host, "example.com");
    EXPECT_EQ(port, 443);
    EXPECT_EQ(path, "/ws");
}

TEST(HttpUtilityTest, IPv6UrlParsing) {
    std::string protocol, host, path;
    uint16_t port = 0;
    EXPECT_TRUE(cpphttp::parse_url("http://[::1]:8080/api/v1", protocol, host, port, path));
    EXPECT_EQ(protocol, "http");
    EXPECT_EQ(host, "::1");
    EXPECT_EQ(port, 8080);
    EXPECT_EQ(path, "/api/v1");

    EXPECT_TRUE(cpphttp::parse_url("https://[2001:db8::1]/index.html", protocol, host, port, path));
    EXPECT_EQ(protocol, "https");
    EXPECT_EQ(host, "2001:db8::1");
    EXPECT_EQ(port, 443);
    EXPECT_EQ(path, "/index.html");
}

TEST(HttpIntegrationTest, AutomaticHEADFallbackAndBodyStripping) {
    cpphttp::HttpServer server(8112);
    server.Get("/test", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("Hello World");
    });
    server.Start();
    
    cpphttp::HttpClient client("127.0.0.1", 8112);
    // With default Accept-Encoding: gzip, HEAD should return compressed headers matching GET
    auto res = client.SendRequest("HEAD", "/test");
    EXPECT_EQ(res.status_code, 200);
    EXPECT_EQ(res.body, "");
    EXPECT_EQ(res.headers.at("Content-Length"), "31");
    EXPECT_EQ(res.headers.at("Content-Encoding"), "gzip");

    // Without compression
    auto res_raw = client.SendRequest("HEAD", "/test", "", cpphttp::HeaderMap{{"Accept-Encoding", "identity"}});
    EXPECT_EQ(res_raw.status_code, 200);
    EXPECT_EQ(res_raw.body, "");
    EXPECT_EQ(res_raw.headers.at("Content-Length"), "11");
    EXPECT_EQ(res_raw.headers.find("Content-Encoding"), res_raw.headers.end());
    
    server.Stop();
}

TEST(HttpIntegrationTest, MalformedRequestRejection) {
    cpphttp::HttpServer server(8113);
    server.Get("/test", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("OK");
    });
    server.Start();
    
    cpptcpnet::TcpClient client;
    auto session = std::make_shared<TestClientSession>();
    client.SetDataHandler([session](uint64_t, const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(session->mutex);
        session->buffer.insert(session->buffer.end(), data.begin(), data.end());
        session->cv.notify_all();
    });
    
    cpppubsub::Worker worker;
    auto sub = client.GetEventBroker().Subscribe<cpptcpnet::ConnectionEvent>("state_events");
    worker.AddSubscription<cpptcpnet::ConnectionEvent>(sub, [&](const cpptcpnet::ConnectionEvent& e) {
        std::lock_guard<std::mutex> lock(session->mutex);
        if (e.state == cpptcpnet::ConnectionState::Connected) session->connected = true;
        else session->disconnected = true;
        session->cv.notify_all();
    });
    worker.Start();
    client.Start();
    
    uint64_t session_id = client.Connect("127.0.0.1", 8113);
    
    {
        std::unique_lock<std::mutex> lock(session->mutex);
        session->cv.wait_for(lock, std::chrono::seconds(5), [&]() { return session->connected; });
    }
    
    // 1. Missing colon in header
    std::string bad_req1 = "GET /test HTTP/1.1\r\nHost localhost\r\n\r\n";
    client.Send(session_id, bad_req1);
    
    {
        std::unique_lock<std::mutex> lock(session->mutex);
        session->cv.wait_for(lock, std::chrono::seconds(5), [&]() {
            std::string s(session->buffer.begin(), session->buffer.end());
            return s.find("HTTP/1.1 400 Bad Request") != std::string::npos;
        });
        std::string s(session->buffer.begin(), session->buffer.end());
        EXPECT_NE(s.find("HTTP/1.1 400 Bad Request"), std::string::npos);
    }
    
    client.Stop();
    worker.Stop();
    server.Stop();
}

TEST(HttpIntegrationTest, RedirectStripsAuthorizationHeader) {
    cpphttp::HttpServer server(8114);
    cpphttp::HttpServer target_server(8115);
    
    server.Get("/redirect", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Redirect("http://127.0.0.1:8115/target", 302);
    });
    
    target_server.Get("/target", [](const cpphttp::HttpRequest &req) {
        std::string auth = req.GetHeader("Authorization");
        return cpphttp::HttpResponse::Plain("Auth: " + auth);
    });
    
    server.Start();
    target_server.Start();
    
    cpphttp::HttpClient client("127.0.0.1", 8114);
    auto res = client.Get("/redirect", {{"Authorization", "Bearer token123"}});
    EXPECT_EQ(res.status_code, 200);
    EXPECT_EQ(res.body, "Auth: ");
    
    server.Stop();
    target_server.Stop();
}

TEST(HttpResponseBuilderTest, ChunkedResponseSuppressesContentLength) {
    cpphttp::HttpResponse res;
    res.status_code = 200;
    res.headers["Transfer-Encoding"] = "chunked";
    res.body = "5\r\nHello\r\n0\r\n\r\n";
    
    std::string serialized = res.Serialize();
    EXPECT_EQ(serialized.find("Content-Length:"), std::string::npos);
    EXPECT_NE(serialized.find("Transfer-Encoding: chunked"), std::string::npos);
}

TEST(HttpServerTest, CustomBindAddress) {
    cpphttp::HttpServer server(8116, "127.0.0.1");
    server.Get("/ping", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("pong");
    });
    
    server.Start();
    
    cpphttp::HttpClient client("127.0.0.1", 8116);
    auto res = client.Get("/ping");
    EXPECT_EQ(res.status_code, 200);
    EXPECT_EQ(res.body, "pong");
    
    server.Stop();
}

// ==========================================
// Extended Coverage Integration & Unit Tests
// ==========================================

TEST(HttpIntegrationExtendedTest, PutDeletePatchOptionsServerAndClient) {
    cpphttp::HttpServer server(8117);
    server.Put("/put", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("PUT: " + req.body);
    });
    server.Delete("/delete", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("DELETE");
    });
    server.Patch("/patch", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("PATCH: " + req.body);
    });
    server.Options("/options", [](const cpphttp::HttpRequest &req) {
        cpphttp::HttpResponse res = cpphttp::HttpResponse::Plain("OPTIONS");
        res.headers["Allow"] = "GET, POST, PUT, DELETE, PATCH, OPTIONS";
        return res;
    });
    server.Start();

    cpphttp::HttpClient client("127.0.0.1", 8117);
    
    // Test PUT
    auto res_put = client.Put("/put", "hello");
    EXPECT_EQ(res_put.status_code, 200);
    EXPECT_EQ(res_put.body, "PUT: hello");

    // Test DELETE
    auto res_del = client.Delete("/delete");
    EXPECT_EQ(res_del.status_code, 200);
    EXPECT_EQ(res_del.body, "DELETE");

    // Test PATCH
    auto res_patch = client.SendRequest("PATCH", "/patch", "world");
    EXPECT_EQ(res_patch.status_code, 200);
    EXPECT_EQ(res_patch.body, "PATCH: world");

    // Test OPTIONS
    auto res_opt = client.SendRequest("OPTIONS", "/options");
    EXPECT_EQ(res_opt.status_code, 200);
    EXPECT_EQ(res_opt.body, "OPTIONS");
    EXPECT_EQ(res_opt.headers.at("Allow"), "GET, POST, PUT, DELETE, PATCH, OPTIONS");

    server.Stop();
}

TEST(HttpIntegrationExtendedTest, RequestHeaderSizeLimit) {
    cpphttp::HttpServer server(8118);
    server.SetMaxHeaderSize(100);
    server.Get("/test", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("OK");
    });
    server.Start();

    cpptcpnet::TcpClient client;
    auto session = std::make_shared<TestClientSession>();
    client.SetDataHandler([session](uint64_t, const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(session->mutex);
        session->buffer.insert(session->buffer.end(), data.begin(), data.end());
        session->cv.notify_all();
    });
    
    cpppubsub::Worker worker;
    auto sub = client.GetEventBroker().Subscribe<cpptcpnet::ConnectionEvent>("state_events");
    worker.AddSubscription<cpptcpnet::ConnectionEvent>(sub, [&](const cpptcpnet::ConnectionEvent& e) {
        std::lock_guard<std::mutex> lock(session->mutex);
        if (e.state == cpptcpnet::ConnectionState::Connected) session->connected = true;
        else session->disconnected = true;
        session->cv.notify_all();
    });
    worker.Start();
    client.Start();
    
    uint64_t session_id = client.Connect("127.0.0.1", 8118);
    {
        std::unique_lock<std::mutex> lock(session->mutex);
        session->cv.wait_for(lock, std::chrono::seconds(5), [&]() { return session->connected; });
    }
    
    std::string large_header(150, 'a');
    std::string req = "GET /test HTTP/1.1\r\nX-Large: " + large_header + "\r\n\r\n";
    client.Send(session_id, req);
    
    {
        std::unique_lock<std::mutex> lock(session->mutex);
        session->cv.wait_for(lock, std::chrono::seconds(5), [&]() {
            std::string s(session->buffer.begin(), session->buffer.end());
            return s.find("431 Request Header Fields Too Large") != std::string::npos;
        });
        std::string s(session->buffer.begin(), session->buffer.end());
        EXPECT_NE(s.find("HTTP/1.1 431 Request Header Fields Too Large"), std::string::npos);
    }
    
    client.Stop();
    worker.Stop();
    server.Stop();
}

TEST(HttpIntegrationExtendedTest, InvalidContentLengthRejection) {
    cpphttp::HttpServer server(8119);
    server.Post("/test", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("OK");
    });
    server.Start();
    
    cpptcpnet::TcpClient client;
    auto session = std::make_shared<TestClientSession>();
    client.SetDataHandler([session](uint64_t, const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(session->mutex);
        session->buffer.insert(session->buffer.end(), data.begin(), data.end());
        session->cv.notify_all();
    });
    
    cpppubsub::Worker worker;
    auto sub = client.GetEventBroker().Subscribe<cpptcpnet::ConnectionEvent>("state_events");
    worker.AddSubscription<cpptcpnet::ConnectionEvent>(sub, [&](const cpptcpnet::ConnectionEvent& e) {
        std::lock_guard<std::mutex> lock(session->mutex);
        if (e.state == cpptcpnet::ConnectionState::Connected) session->connected = true;
        else session->disconnected = true;
        session->cv.notify_all();
    });
    worker.Start();
    client.Start();
    
    uint64_t session_id = client.Connect("127.0.0.1", 8119);
    {
        std::unique_lock<std::mutex> lock(session->mutex);
        session->cv.wait_for(lock, std::chrono::seconds(5), [&]() { return session->connected; });
    }
    
    std::string req = "POST /test HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: abc\r\n\r\nhello";
    client.Send(session_id, req);
    
    {
        std::unique_lock<std::mutex> lock(session->mutex);
        session->cv.wait_for(lock, std::chrono::seconds(5), [&]() {
            std::string s(session->buffer.begin(), session->buffer.end());
            return s.find("400 Bad Request") != std::string::npos;
        });
        std::string s(session->buffer.begin(), session->buffer.end());
        EXPECT_NE(s.find("HTTP/1.1 400 Bad Request"), std::string::npos);
    }
    
    client.Stop();
    worker.Stop();
    server.Stop();
}

TEST(HttpIntegrationExtendedTest, DuplicateContentLengthRejection) {
    cpphttp::HttpServer server(8120);
    server.Post("/test", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("OK");
    });
    server.Start();
    
    cpptcpnet::TcpClient client;
    auto session = std::make_shared<TestClientSession>();
    client.SetDataHandler([session](uint64_t, const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(session->mutex);
        session->buffer.insert(session->buffer.end(), data.begin(), data.end());
        session->cv.notify_all();
    });
    
    cpppubsub::Worker worker;
    auto sub = client.GetEventBroker().Subscribe<cpptcpnet::ConnectionEvent>("state_events");
    worker.AddSubscription<cpptcpnet::ConnectionEvent>(sub, [&](const cpptcpnet::ConnectionEvent& e) {
        std::lock_guard<std::mutex> lock(session->mutex);
        if (e.state == cpptcpnet::ConnectionState::Connected) session->connected = true;
        else session->disconnected = true;
        session->cv.notify_all();
    });
    worker.Start();
    client.Start();
    
    uint64_t session_id = client.Connect("127.0.0.1", 8120);
    {
        std::unique_lock<std::mutex> lock(session->mutex);
        session->cv.wait_for(lock, std::chrono::seconds(5), [&]() { return session->connected; });
    }
    
    std::string req = "POST /test HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: 5\r\nContent-Length: 5\r\n\r\nhello";
    client.Send(session_id, req);
    
    {
        std::unique_lock<std::mutex> lock(session->mutex);
        session->cv.wait_for(lock, std::chrono::seconds(5), [&]() {
            std::string s(session->buffer.begin(), session->buffer.end());
            return s.find("Malformed HTTP request.") != std::string::npos;
        });
        std::string s(session->buffer.begin(), session->buffer.end());
        EXPECT_NE(s.find("HTTP/1.1 400 Bad Request"), std::string::npos);
        EXPECT_NE(s.find("Malformed HTTP request."), std::string::npos);
    }
    
    client.Stop();
    worker.Stop();
    server.Stop();
}

TEST(HttpIntegrationExtendedTest, ClientRedirectLoop) {
    cpphttp::HttpServer server(8121);
    server.Get("/loop", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Redirect("http://127.0.0.1:8121/loop", 302);
    });
    server.Start();
    
    cpphttp::HttpClient client("127.0.0.1", 8121);
    client.SetMaxRedirects(3);
    EXPECT_EQ(client.GetMaxRedirects(), 3);
    
    EXPECT_THROW({
        try {
            client.Get("/loop");
        } catch (const std::runtime_error &e) {
            EXPECT_STREQ(e.what(), "Too many redirects");
            throw;
        }
    }, std::runtime_error);
    
    server.Stop();
}

TEST(WebSocketIntegrationExtendedTest, BinaryFrameEcho) {
    cpphttp::HttpServer server(8122);
    
    cpphttp::WebSocketBehavior ws_behavior;
    ws_behavior.on_open = [](std::shared_ptr<cpphttp::WebSocketConnection> conn) {};
    ws_behavior.on_binary = [](std::shared_ptr<cpphttp::WebSocketConnection> conn, const std::vector<uint8_t> &data) {
        conn->SendBinary(data);
    };
    server.WebSocket("/ws", std::move(ws_behavior));
    
    server.Start();

    cpphttp::WebSocketClient client;
    std::mutex ws_mtx;
    std::condition_variable ws_cv;
    std::vector<uint8_t> received_data;
    bool opened = false;

    client.OnOpen([&]() {
        std::lock_guard<std::mutex> lock(ws_mtx);
        opened = true;
        ws_cv.notify_all();
    });

    client.OnBinary([&](const std::vector<uint8_t> &data) {
        std::lock_guard<std::mutex> lock(ws_mtx);
        received_data = data;
        ws_cv.notify_all();
    });

    ASSERT_TRUE(client.Connect("ws://127.0.0.1:8122/ws"));

    {
        std::unique_lock<std::mutex> lock(ws_mtx);
        ws_cv.wait_for(lock, std::chrono::seconds(5), [&]() { return opened; });
        ASSERT_TRUE(opened);
    }

    std::vector<uint8_t> test_payload = {0xDE, 0xAD, 0xBE, 0xEF};
    client.SendBinary(test_payload);

    {
        std::unique_lock<std::mutex> lock(ws_mtx);
        ws_cv.wait_for(lock, std::chrono::seconds(5), [&]() { return !received_data.empty(); });
        EXPECT_EQ(received_data, test_payload);
    }

    client.Close();
    server.Stop();
}

TEST(WebSocketIntegrationExtendedTest, FragmentedMessageText) {
    cpphttp::HttpServer server(8123);
    cpphttp::WebSocketBehavior ws_behavior;
    ws_behavior.on_open = [](std::shared_ptr<cpphttp::WebSocketConnection> conn) {};
    ws_behavior.on_message = [](std::shared_ptr<cpphttp::WebSocketConnection> conn, const std::string &msg) {
        conn->Send("Echo: " + msg);
    };
    server.WebSocket("/ws", std::move(ws_behavior));
    server.Start();

    cpptcpnet::TcpClient client;
    auto session = std::make_shared<TestClientSession>();
    
    client.SetDataHandler([session](uint64_t, const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(session->mutex);
        session->buffer.insert(session->buffer.end(), data.begin(), data.end());
        session->cv.notify_all();
    });
    
    cpppubsub::Worker worker;
    auto sub = client.GetEventBroker().Subscribe<cpptcpnet::ConnectionEvent>("state_events");
    worker.AddSubscription<cpptcpnet::ConnectionEvent>(sub, [&](const cpptcpnet::ConnectionEvent& e) {
        std::lock_guard<std::mutex> lock(session->mutex);
        if (e.state == cpptcpnet::ConnectionState::Connected) session->connected = true;
        else session->disconnected = true;
        session->cv.notify_all();
    });
    worker.Start();
    client.Start();
    
    uint64_t session_id = client.Connect("127.0.0.1", 8123);
    {
        std::unique_lock<std::mutex> lock(session->mutex);
        session->cv.wait_for(lock, std::chrono::seconds(5), [&]() { return session->connected; });
    }
    
    ASSERT_TRUE(PerformWsHandshake(session, client, session_id, "/ws"));

    std::vector<uint8_t> frag1 = {
        0x01, // FIN=0, Opcode=1
        0x86, // Mask=1, Len=6
        0x11, 0x22, 0x33, 0x44
    };
    std::string p1 = "Hello ";
    for (size_t i = 0; i < p1.size(); ++i) {
        frag1.push_back(p1[i] ^ (i % 4 == 0 ? 0x11 : i % 4 == 1 ? 0x22 : i % 4 == 2 ? 0x33 : 0x44));
    }
    client.Send(session_id, frag1);

    std::vector<uint8_t> frag2 = {
        0x80, // FIN=1, Opcode=0
        0x85, // Mask=1, Len=5
        0x11, 0x22, 0x33, 0x44
    };
    std::string p2 = "World";
    for (size_t i = 0; i < p2.size(); ++i) {
        frag2.push_back(p2[i] ^ (i % 4 == 0 ? 0x11 : i % 4 == 1 ? 0x22 : i % 4 == 2 ? 0x33 : 0x44));
    }
    client.Send(session_id, frag2);

    std::string response_text;
    ASSERT_TRUE(ExpectWsTextFrame(session, response_text));
    EXPECT_EQ(response_text, "Echo: Hello World");

    client.Stop();
    worker.Stop();
    server.Stop();
}

TEST(WebSocketIntegrationExtendedTest, PingPong) {
    cpphttp::HttpServer server(8124);
    cpphttp::WebSocketBehavior ws_behavior;
    ws_behavior.on_open = [](std::shared_ptr<cpphttp::WebSocketConnection> conn) {};
    server.WebSocket("/ws", std::move(ws_behavior));
    server.Start();

    cpptcpnet::TcpClient client;
    auto session = std::make_shared<TestClientSession>();
    
    client.SetDataHandler([session](uint64_t, const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(session->mutex);
        session->buffer.insert(session->buffer.end(), data.begin(), data.end());
        session->cv.notify_all();
    });
    
    cpppubsub::Worker worker;
    auto sub = client.GetEventBroker().Subscribe<cpptcpnet::ConnectionEvent>("state_events");
    worker.AddSubscription<cpptcpnet::ConnectionEvent>(sub, [&](const cpptcpnet::ConnectionEvent& e) {
        std::lock_guard<std::mutex> lock(session->mutex);
        if (e.state == cpptcpnet::ConnectionState::Connected) session->connected = true;
        else session->disconnected = true;
        session->cv.notify_all();
    });
    worker.Start();
    client.Start();
    
    uint64_t session_id = client.Connect("127.0.0.1", 8124);
    {
        std::unique_lock<std::mutex> lock(session->mutex);
        session->cv.wait_for(lock, std::chrono::seconds(5), [&]() { return session->connected; });
    }
    
    ASSERT_TRUE(PerformWsHandshake(session, client, session_id, "/ws"));

    std::vector<uint8_t> ping_frame = {
        0x89, // FIN=1, Opcode=9
        0x84, // Mask=1, Len=4
        0x11, 0x22, 0x33, 0x44
    };
    std::string ping_payload = "ping";
    for (size_t i = 0; i < ping_payload.size(); ++i) {
        ping_frame.push_back(ping_payload[i] ^ (i % 4 == 0 ? 0x11 : i % 4 == 1 ? 0x22 : i % 4 == 2 ? 0x33 : 0x44));
    }
    client.Send(session_id, ping_frame);

    {
        std::unique_lock<std::mutex> lock(session->mutex);
        auto has_pong = [&]() {
            if (session->buffer.size() < 6) return false;
            uint8_t opcode = session->buffer[0] & 0x0F;
            uint8_t len = session->buffer[1] & 0x7F;
            return opcode == 0x0A && len == 4 && session->buffer.size() >= 6;
        };
        ASSERT_TRUE(session->cv.wait_for(lock, std::chrono::seconds(5), has_pong));
        EXPECT_EQ(session->buffer[0], 0x8A);
        EXPECT_EQ(session->buffer[1], 4);
        std::string pong_payload(session->buffer.begin() + 2, session->buffer.begin() + 6);
        EXPECT_EQ(pong_payload, "ping");
    }

    client.Stop();
    worker.Stop();
    server.Stop();
}

TEST(WebSocketIntegrationExtendedTest, PayloadTooLarge) {
    cpphttp::HttpServer server(8125);
    server.SetMaxBodySize(10);
    
    cpphttp::WebSocketBehavior ws_behavior;
    ws_behavior.on_open = [](std::shared_ptr<cpphttp::WebSocketConnection> conn) {};
    server.WebSocket("/ws", std::move(ws_behavior));
    server.Start();

    cpptcpnet::TcpClient client;
    auto session = std::make_shared<TestClientSession>();
    
    client.SetDataHandler([session](uint64_t, const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(session->mutex);
        session->buffer.insert(session->buffer.end(), data.begin(), data.end());
        session->cv.notify_all();
    });
    
    cpppubsub::Worker worker;
    auto sub = client.GetEventBroker().Subscribe<cpptcpnet::ConnectionEvent>("state_events");
    worker.AddSubscription<cpptcpnet::ConnectionEvent>(sub, [&](const cpptcpnet::ConnectionEvent& e) {
        std::lock_guard<std::mutex> lock(session->mutex);
        if (e.state == cpptcpnet::ConnectionState::Connected) session->connected = true;
        else session->disconnected = true;
        session->cv.notify_all();
    });
    worker.Start();
    client.Start();
    
    uint64_t session_id = client.Connect("127.0.0.1", 8125);
    {
        std::unique_lock<std::mutex> lock(session->mutex);
        session->cv.wait_for(lock, std::chrono::seconds(5), [&]() { return session->connected; });
    }
    
    ASSERT_TRUE(PerformWsHandshake(session, client, session_id, "/ws"));

    ASSERT_TRUE(SendWsTextFrame(client, session_id, "123456789012345"));

    {
        std::unique_lock<std::mutex> lock(session->mutex);
        auto has_close = [&]() {
            if (session->buffer.size() < 4) return false;
            uint8_t opcode = session->buffer[0] & 0x0F;
            return opcode == 0x08;
        };
        ASSERT_TRUE(session->cv.wait_for(lock, std::chrono::seconds(5), has_close));
        
        uint16_t status_code = (session->buffer[2] << 8) | session->buffer[3];
        EXPECT_EQ(status_code, 1009);
    }

    client.Stop();
    worker.Stop();
    server.Stop();
}

TEST(WebSocketIntegrationExtendedTest, PathParameters) {
    cpphttp::HttpServer server(8126);
    
    cpphttp::WebSocketBehavior ws_behavior;
    bool opened = false;
    ws_behavior.on_open = [&](std::shared_ptr<cpphttp::WebSocketConnection> conn) {
        opened = true;
    };
    server.WebSocket("/ws/:name", std::move(ws_behavior));
    server.Start();

    cpphttp::WebSocketClient client;
    ASSERT_TRUE(client.Connect("ws://127.0.0.1:8126/ws/john"));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_TRUE(opened);

    client.Close();
    server.Stop();
}

TEST(WebSocketIntegrationExtendedTest, InvalidUpgradeRequest) {
    cpphttp::HttpServer server(8127);
    cpphttp::WebSocketBehavior ws_behavior;
    server.WebSocket("/ws", std::move(ws_behavior));
    server.Start();

    cpphttp::HttpClient client("127.0.0.1", 8127);
    auto res = client.Get("/ws");
    EXPECT_EQ(res.status_code, 400);
    EXPECT_EQ(res.body, "Invalid WebSocket upgrade request.");

    server.Stop();
}

TEST(HttpIntegrationExtendedTest, ClientConnectionTimeout) {
    cpphttp::HttpClient client("192.0.2.1", 80);
    client.SetTimeout(std::chrono::milliseconds(20));
    
    EXPECT_THROW({
        try {
            client.Get("/");
        } catch (const std::runtime_error &e) {
            std::string msg = e.what();
            EXPECT_TRUE(msg == "Connection timeout" || msg == "Failed to connect to server" || msg == "Connection failed during handshake");
            throw;
        }
    }, std::runtime_error);
}

TEST(HttpIntegrationExtendedTest, ClientRequestTimeout) {
    cpphttp::HttpServer server(8128);
    server.Get("/slow", [](const cpphttp::HttpRequest &req) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        return cpphttp::HttpResponse::Plain("Too slow");
    });
    server.Start();

    cpphttp::HttpClient client("127.0.0.1", 8128);
    client.SetTimeout(std::chrono::milliseconds(50));

    EXPECT_THROW({
        try {
            client.Get("/slow");
        } catch (const std::runtime_error &e) {
            EXPECT_STREQ(e.what(), "Request timed out");
            throw;
        }
    }, std::runtime_error);

    server.Stop();
}

TEST(HttpIntegrationExtendedTest, ClientMaxBodySize) {
    cpphttp::HttpServer server(8129);
    server.Get("/data", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("1234567890");
    });
    server.Start();

    cpphttp::HttpClient client("127.0.0.1", 8129);
    client.SetMaxBodySize(5);
    EXPECT_EQ(client.GetMaxBodySize(), 5);

    EXPECT_THROW({
        try {
            client.Get("/data");
        } catch (const std::runtime_error &e) {
            EXPECT_STREQ(e.what(), "Connection closed before response was fully received");
            throw;
        }
    }, std::runtime_error);

    server.Stop();
}

TEST(HttpIntegrationExtendedTest, RedirectPreservesMethodOn307And308) {
    cpphttp::HttpServer server(8130);
    
    server.Post("/redirect307", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Redirect("http://127.0.0.1:8130/target", 307);
    });
    server.Post("/redirect308", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Redirect("http://127.0.0.1:8130/target", 308);
    });
    server.Post("/target", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("Method: " + req.method + ", Body: " + req.body);
    });
    
    server.Start();

    cpphttp::HttpClient client("127.0.0.1", 8130);
    
    auto res307 = client.Post("/redirect307", "payload307");
    EXPECT_EQ(res307.status_code, 200);
    EXPECT_EQ(res307.body, "Method: POST, Body: payload307");

    auto res308 = client.Post("/redirect308", "payload308");
    EXPECT_EQ(res308.status_code, 200);
    EXPECT_EQ(res308.body, "Method: POST, Body: payload308");

    server.Stop();
}

TEST(HttpIntegrationExtendedTest, ServerHandlerThrows) {
    cpphttp::HttpServer server(8131);
    server.Get("/throw", [](const cpphttp::HttpRequest &req) {
        throw std::runtime_error("Something went wrong");
        return cpphttp::HttpResponse::Plain("OK");
    });
    server.Start();

    cpphttp::HttpClient client("127.0.0.1", 8131);
    ASSERT_NO_THROW({
        auto res = client.Get("/throw");
        EXPECT_EQ(res.status_code, 500);
        EXPECT_EQ(res.body, "500 Internal Server Error");
    });

    server.Stop();
}

TEST(HttpUtilityExtendedTest, ParseUrlEdgeCases) {
    std::string protocol, host, path;
    uint16_t port;

    EXPECT_TRUE(cpphttp::parse_url("localhost:8080/foo", protocol, host, port, path));
    EXPECT_EQ(protocol, "http");
    EXPECT_EQ(host, "localhost");
    EXPECT_EQ(port, 8080);
    EXPECT_EQ(path, "/foo");

    EXPECT_TRUE(cpphttp::parse_url("https://localhost:8443", protocol, host, port, path));
    EXPECT_EQ(path, "/");

    EXPECT_TRUE(cpphttp::parse_url("http://[2001:db8::1]", protocol, host, port, path));
    EXPECT_EQ(protocol, "http");
    EXPECT_EQ(host, "2001:db8::1");
    EXPECT_EQ(port, 80);
    EXPECT_EQ(path, "/");

    EXPECT_TRUE(cpphttp::parse_url("https://[2001:db8::1]:8443/test", protocol, host, port, path));
    EXPECT_EQ(protocol, "https");
    EXPECT_EQ(host, "2001:db8::1");
    EXPECT_EQ(port, 8443);
    EXPECT_EQ(path, "/test");

    EXPECT_FALSE(cpphttp::parse_url("http://localhost:65536/foo", protocol, host, port, path));
    EXPECT_FALSE(cpphttp::parse_url("http://[2001:db8::1]:70000/foo", protocol, host, port, path));

    EXPECT_FALSE(cpphttp::parse_url("http://localhost:abc/foo", protocol, host, port, path));
    EXPECT_FALSE(cpphttp::parse_url("http://[2001:db8::1]:abc/foo", protocol, host, port, path));

    EXPECT_TRUE(cpphttp::parse_url("http://localhost/path#section1", protocol, host, port, path));
    EXPECT_EQ(path, "/path");

    EXPECT_TRUE(cpphttp::parse_url("http://[2001:db8::1]/path#section2", protocol, host, port, path));
    EXPECT_EQ(path, "/path");
}

TEST(HttpUtilityExtendedTest, UrlDecodeEdgeCases) {
    EXPECT_EQ(cpphttp::url_decode("a+b", false), "a+b");
    EXPECT_EQ(cpphttp::url_decode("a+b", true), "a b");

    EXPECT_EQ(cpphttp::url_decode("a%"), "a%");
    EXPECT_EQ(cpphttp::url_decode("a%2"), "a%2");

    EXPECT_EQ(cpphttp::url_decode("a%ZZ"), "a%ZZ");
    EXPECT_EQ(cpphttp::url_decode("a%1Z"), "a%1Z");
}

TEST(HttpResponseBuilderExtendedTest, FactoryHelpers) {
    auto res_html = cpphttp::HttpResponse::Html("<h1>Hello</h1>", 201);
    EXPECT_EQ(res_html.status_code, 201);
    EXPECT_EQ(res_html.status_message, "Created");
    EXPECT_EQ(res_html.headers.at("Content-Type"), "text/html");
    EXPECT_EQ(res_html.body, "<h1>Hello</h1>");

    auto res_plain = cpphttp::HttpResponse::Plain("Plain text", 404);
    EXPECT_EQ(res_plain.status_code, 404);
    EXPECT_EQ(res_plain.status_message, "Not Found");
    EXPECT_EQ(res_plain.headers.at("Content-Type"), "text/plain");
    EXPECT_EQ(res_plain.body, "Plain text");
}

TEST(HttpResponseBuilderExtendedTest, GetDefaultStatusMessage) {
    EXPECT_EQ(cpphttp::HttpResponse::GetDefaultStatusMessage(100), "Continue");
    EXPECT_EQ(cpphttp::HttpResponse::GetDefaultStatusMessage(101), "Switching Protocols");
    EXPECT_EQ(cpphttp::HttpResponse::GetDefaultStatusMessage(200), "OK");
    EXPECT_EQ(cpphttp::HttpResponse::GetDefaultStatusMessage(201), "Created");
    EXPECT_EQ(cpphttp::HttpResponse::GetDefaultStatusMessage(202), "Accepted");
    EXPECT_EQ(cpphttp::HttpResponse::GetDefaultStatusMessage(204), "No Content");
    EXPECT_EQ(cpphttp::HttpResponse::GetDefaultStatusMessage(301), "Moved Permanently");
    EXPECT_EQ(cpphttp::HttpResponse::GetDefaultStatusMessage(302), "Found");
    EXPECT_EQ(cpphttp::HttpResponse::GetDefaultStatusMessage(304), "Not Modified");
    EXPECT_EQ(cpphttp::HttpResponse::GetDefaultStatusMessage(400), "Bad Request");
    EXPECT_EQ(cpphttp::HttpResponse::GetDefaultStatusMessage(401), "Unauthorized");
    EXPECT_EQ(cpphttp::HttpResponse::GetDefaultStatusMessage(403), "Forbidden");
    EXPECT_EQ(cpphttp::HttpResponse::GetDefaultStatusMessage(404), "Not Found");
    EXPECT_EQ(cpphttp::HttpResponse::GetDefaultStatusMessage(405), "Method Not Allowed");
    EXPECT_EQ(cpphttp::HttpResponse::GetDefaultStatusMessage(413), "Payload Too Large");
    EXPECT_EQ(cpphttp::HttpResponse::GetDefaultStatusMessage(431), "Request Header Fields Too Large");
    EXPECT_EQ(cpphttp::HttpResponse::GetDefaultStatusMessage(500), "Internal Server Error");
    EXPECT_EQ(cpphttp::HttpResponse::GetDefaultStatusMessage(501), "Not Implemented");
    EXPECT_EQ(cpphttp::HttpResponse::GetDefaultStatusMessage(502), "Bad Gateway");
    EXPECT_EQ(cpphttp::HttpResponse::GetDefaultStatusMessage(503), "Service Unavailable");
    EXPECT_EQ(cpphttp::HttpResponse::GetDefaultStatusMessage(999), "Unknown");
}

TEST(WsFrameTest, ParseWsFrame16BitLength) {
    std::vector<uint8_t> buffer = {
        0x81,
        0xFE,
        0x00, 0x80,
        0x11, 0x22, 0x33, 0x44
    };
    for (int i = 0; i < 128; ++i) {
        uint8_t mask_byte = (i % 4 == 0 ? 0x11 : i % 4 == 1 ? 0x22 : i % 4 == 2 ? 0x33 : 0x44);
        buffer.push_back(0x55 ^ mask_byte);
    }

    cpphttp::WsFrame frame;
    bool protocol_error = false;
    bool parsed = cpphttp::ParseWsFrame(buffer, frame, protocol_error);

    EXPECT_TRUE(parsed);
    EXPECT_FALSE(protocol_error);
    EXPECT_EQ(frame.payload.size(), 128);
    for (uint8_t byte : frame.payload) {
        EXPECT_EQ(byte, 0x55);
    }
}

TEST(WsFrameTest, ParseWsFrame64BitLength) {
    std::vector<uint8_t> buffer = {
        0x81,
        0xFF,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01,
        0x11, 0x22, 0x33, 0x44
    };
    for (int i = 0; i < 65537; ++i) {
        uint8_t mask_byte = (i % 4 == 0 ? 0x11 : i % 4 == 1 ? 0x22 : i % 4 == 2 ? 0x33 : 0x44);
        buffer.push_back(0xAA ^ mask_byte);
    }

    cpphttp::WsFrame frame;
    bool protocol_error = false;
    bool parsed = cpphttp::ParseWsFrame(buffer, frame, protocol_error);

    EXPECT_TRUE(parsed);
    EXPECT_FALSE(protocol_error);
    EXPECT_EQ(frame.payload.size(), 65537);
    EXPECT_EQ(frame.payload[0], 0xAA);
    EXPECT_EQ(frame.payload[65536], 0xAA);
}

TEST(WsFrameTest, RSVBitsSetFails) {
    std::vector<uint8_t> buffer = {
        0xC1,
        0x85,
        0x11, 0x22, 0x33, 0x44,
        0x59, 0x47, 0x5F, 0x28, 0x7E
    };

    cpphttp::WsFrame frame;
    bool protocol_error = false;
    bool parsed = cpphttp::ParseWsFrame(buffer, frame, protocol_error);

    EXPECT_FALSE(parsed);
    EXPECT_TRUE(protocol_error);
}

TEST(HttpMultipartTest, ParseMultipartEmptyBoundaryAndMultipleParts) {
    auto empty_parts = cpphttp::ParseMultipart("body", "");
    EXPECT_TRUE(empty_parts.empty());

    std::string boundary = "Boundary456";
    std::string body = 
        "--Boundary456\r\n"
        "Content-Disposition: form-data; name=\"part1\"\r\n\r\n"
        "value1\r\n"
        "--Boundary456\r\n"
        "Content-Disposition: form-data; name=\"part2\"; filename=\"test.txt\"\r\n"
        "Content-Type: text/plain\r\n\r\n"
        "value2\r\n"
        "--Boundary456\r\n"
        "Content-Disposition: form-data; name=\"part3\"\r\n"
        "Content-Type: application/octet-stream\r\n\r\n"
        "value3\r\n"
        "--Boundary456--\r\n";

    auto parts = cpphttp::ParseMultipart(body, boundary);
    ASSERT_EQ(parts.size(), 3);

    EXPECT_EQ(parts[0].name, "part1");
    EXPECT_EQ(parts[0].filename, "");
    EXPECT_EQ(parts[0].content_type, "");
    EXPECT_EQ(parts[0].data, "value1");

    EXPECT_EQ(parts[1].name, "part2");
    EXPECT_EQ(parts[1].filename, "test.txt");
    EXPECT_EQ(parts[1].content_type, "text/plain");
    EXPECT_EQ(parts[1].data, "value2");

    EXPECT_EQ(parts[2].name, "part3");
    EXPECT_EQ(parts[2].filename, "");
    EXPECT_EQ(parts[2].content_type, "application/octet-stream");
    EXPECT_EQ(parts[2].data, "value3");
}

TEST(HttpCompressionTest, GzipFailurePaths) {
    std::string corrupt_data = "this is not gzipped data!";
    auto decompressed = cpphttp::gzip_decompress(corrupt_data);
    EXPECT_FALSE(decompressed.has_value());
}

TEST(HttpUtilityExtendedTest, VersionString) {
    std::string ver = cpphttp::version();
    EXPECT_FALSE(ver.empty());
    EXPECT_TRUE(ver.find('.') != std::string::npos);
}

TEST(HttpIntegrationExtendedTest, Http10CloseSemantics) {
    cpphttp::HttpServer server(8132);
    server.Get("/ping", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("pong");
    });
    server.Start();

    cpptcpnet::TcpClient client;
    auto session = std::make_shared<TestClientSession>();
    
    client.SetDataHandler([session](uint64_t, const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(session->mutex);
        session->buffer.insert(session->buffer.end(), data.begin(), data.end());
        session->cv.notify_all();
    });
    
    cpppubsub::Worker worker;
    auto sub = client.GetEventBroker().Subscribe<cpptcpnet::ConnectionEvent>("state_events");
    worker.AddSubscription<cpptcpnet::ConnectionEvent>(sub, [&](const cpptcpnet::ConnectionEvent& e) {
        std::lock_guard<std::mutex> lock(session->mutex);
        if (e.state == cpptcpnet::ConnectionState::Connected) session->connected = true;
        else session->disconnected = true;
        session->cv.notify_all();
    });
    worker.Start();
    client.Start();
    
    uint64_t session_id = client.Connect("127.0.0.1", 8132);
    {
        std::unique_lock<std::mutex> lock(session->mutex);
        session->cv.wait_for(lock, std::chrono::seconds(5), [&]() { return session->connected; });
    }
    
    std::string req = "GET /ping HTTP/1.0\r\nHost: 127.0.0.1\r\n\r\n";
    client.Send(session_id, req);
    
    {
        std::unique_lock<std::mutex> lock(session->mutex);
        session->cv.wait_for(lock, std::chrono::seconds(5), [&]() {
            std::string s(session->buffer.begin(), session->buffer.end());
            return s.find("\r\n\r\n") != std::string::npos;
        });
        std::string s(session->buffer.begin(), session->buffer.end());
        EXPECT_NE(s.find("Connection: close"), std::string::npos);
        EXPECT_NE(s.find("pong"), std::string::npos);
    }
    
    {
        std::unique_lock<std::mutex> lock(session->mutex);
        session->cv.wait_for(lock, std::chrono::seconds(5), [&]() { return session->disconnected; });
        EXPECT_TRUE(session->disconnected);
    }
    
    client.Stop();
    worker.Stop();
    server.Stop();
}

TEST(HttpIntegrationExtendedTest, AsyncClientMethods) {
    cpphttp::HttpServer server(8133);
    server.Put("/put", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("PUT: " + req.body);
    });
    server.Delete("/delete", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("DELETE");
    });
    server.Post("/post", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("POST: " + req.body);
    });
    server.Start();

    cpphttp::HttpClient client("127.0.0.1", 8133);

    auto fut_put = client.PutAsync("/put", "hello");
    auto fut_del = client.DeleteAsync("/delete");
    auto fut_post = client.PostAsync("/post", "world");

    auto res_put = fut_put.get();
    EXPECT_EQ(res_put.status_code, 200);
    EXPECT_EQ(res_put.body, "PUT: hello");

    auto res_del = fut_del.get();
    EXPECT_EQ(res_del.status_code, 200);
    EXPECT_EQ(res_del.body, "DELETE");

    auto res_post = fut_post.get();
    EXPECT_EQ(res_post.status_code, 200);
    EXPECT_EQ(res_post.body, "POST: world");

    server.Stop();
}

TEST(HttpConfigTest, SetClientWorkerPoolSize) {
    cpphttp::SetClientWorkerPoolSize(8);
    EXPECT_EQ(cpphttp::GetClientWorkerPoolSize(), 8);
}

TEST(HttpIntegrationExtendedTest, QueryParamsNoValue) {
    cpphttp::HttpServer server(8134);
    server.Get("/query", [](const cpphttp::HttpRequest &req) {
        std::string res;
        for (const auto &pair : req.query_params) {
            res += pair.first + ":" + pair.second + ";";
        }
        return cpphttp::HttpResponse::Plain(res);
    });
    server.Start();

    cpphttp::HttpClient client("127.0.0.1", 8134);
    auto res = client.Get("/query?key&foo=bar&baz");
    EXPECT_EQ(res.status_code, 200);
    EXPECT_TRUE(res.body.find("key:;") != std::string::npos);
    EXPECT_TRUE(res.body.find("foo:bar;") != std::string::npos);
    EXPECT_TRUE(res.body.find("baz:;") != std::string::npos);

    server.Stop();
}

TEST(HttpUtilityExtendedTest, HttpRequestGetHeaderMissingKey) {
    cpphttp::HttpRequest req;
    req.headers["Content-Type"] = "text/plain";
    EXPECT_EQ(req.GetHeader("Content-Type"), "text/plain");
    EXPECT_EQ(req.GetHeader("X-Missing-Header"), "");
}

TEST(HttpIntegrationExtendedTest, ConcurrencyStress) {
    cpphttp::HttpServer server(8135);
    server.Get("/ping", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("pong");
    });
    server.Start();

    const int num_threads = 10;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            try {
                cpphttp::HttpClient client("127.0.0.1", 8135);
                for (int j = 0; j < 5; ++j) {
                    auto res = client.Get("/ping");
                    if (res.status_code == 200 && res.body == "pong") {
                        success_count++;
                    }
                }
            } catch (...) {
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), num_threads * 5);

    server.Stop();
}

#if defined(CPPTCPNET_SSL_SUPPORT)
TEST(WebSocketIntegrationTest, WebSocketClientSecureEcho) {
    cpphttp::HttpServer server(8136);
    
    cpptcpnet::TcpListener::SslConfig ssl_config;
    ssl_config.cert_file = CPPHTTP_CERTS_DIR "/server.crt";
    ssl_config.key_file = CPPHTTP_CERTS_DIR "/server.key";
    
    ASSERT_NO_THROW(server.GetListener().EnableSSL(ssl_config));
    
    cpphttp::WebSocketBehavior ws_behavior;
    ws_behavior.on_open = [](std::shared_ptr<cpphttp::WebSocketConnection> conn) {};
    ws_behavior.on_message = [](std::shared_ptr<cpphttp::WebSocketConnection> conn, const std::string &msg) {
        conn->Send("Secure Echo: " + msg);
    };
    server.WebSocket("/ws", std::move(ws_behavior));
    
    server.Start();

    cpphttp::WebSocketClient client;
    std::mutex ws_mtx;
    std::condition_variable ws_cv;
    std::string received_msg;
    bool opened = false;

    client.OnOpen([&]() {
        std::lock_guard<std::mutex> lock(ws_mtx);
        opened = true;
        ws_cv.notify_all();
    });

    client.OnMessage([&](const std::string &msg) {
        std::lock_guard<std::mutex> lock(ws_mtx);
        received_msg = msg;
        ws_cv.notify_all();
    });

    ASSERT_TRUE(client.Connect("wss://127.0.0.1:8136/ws"));

    {
        std::unique_lock<std::mutex> lock(ws_mtx);
        ws_cv.wait_for(lock, std::chrono::seconds(5), [&]() { return opened; });
        ASSERT_TRUE(opened);
    }

    client.Send("Hello Secure WebSocket");

    {
        std::unique_lock<std::mutex> lock(ws_mtx);
        ws_cv.wait_for(lock, std::chrono::seconds(5), [&]() { return !received_msg.empty(); });
        EXPECT_EQ(received_msg, "Secure Echo: Hello Secure WebSocket");
    }

    client.Close();
    server.Stop();
}
#endif

TEST(HttpIntegrationTest, Redirect303MethodDowngrade) {
    cpphttp::HttpServer server(8140);

    server.Post("/redirect303", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Redirect("/target303", 303);
    });

    server.Get("/target303", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("GET 303 Target Reached");
    });

    server.Post("/target303", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("POST 303 Target Reached");
    });

    server.Start();

    cpphttp::HttpClient client("127.0.0.1", 8140);
    auto res = client.Post("/redirect303", "post_body");
    EXPECT_EQ(res.status_code, 200);
    EXPECT_EQ(res.body, "GET 303 Target Reached");

    server.Stop();
}

TEST(HttpIntegrationTest, SmugglingDuplicateTransferEncoding) {
    cpphttp::HttpServer server(8141);
    server.Get("/test", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("OK");
    });
    server.Start();

    cpptcpnet::TcpClient client;
    auto session = std::make_shared<TestClientSession>();
    client.SetDataHandler([session](uint64_t, const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(session->mutex);
        session->buffer.insert(session->buffer.end(), data.begin(), data.end());
        session->cv.notify_all();
    });

    cpppubsub::Worker worker;
    auto sub = client.GetEventBroker().Subscribe<cpptcpnet::ConnectionEvent>("state_events");
    worker.AddSubscription<cpptcpnet::ConnectionEvent>(sub, [&](const cpptcpnet::ConnectionEvent& e) {
        std::lock_guard<std::mutex> lock(session->mutex);
        if (e.state == cpptcpnet::ConnectionState::Connected) session->connected = true;
        else session->disconnected = true;
        session->cv.notify_all();
    });
    worker.Start();
    client.Start();

    uint64_t session_id = client.Connect("127.0.0.1", 8141);

    {
        std::unique_lock<std::mutex> lock(session->mutex);
        session->cv.wait_for(lock, std::chrono::seconds(5), [&]() { return session->connected; });
    }

    std::string smugg_req = 
        "POST /test HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Transfer-Encoding: identity\r\n"
        "Content-Length: 5\r\n\r\n"
        "0\r\n\r\n";
    client.Send(session_id, smugg_req);

    {
        std::unique_lock<std::mutex> lock(session->mutex);
        session->cv.wait_for(lock, std::chrono::seconds(5), [&]() {
            std::string s(session->buffer.begin(), session->buffer.end());
            return s.find("HTTP/1.1 400 Bad Request") != std::string::npos;
        });
        std::string s(session->buffer.begin(), session->buffer.end());
        EXPECT_NE(s.find("HTTP/1.1 400 Bad Request"), std::string::npos);
    }

    client.Stop();
    worker.Stop();
    server.Stop();
}

TEST(HttpIntegrationTest, SmugglingInvalidTransferEncoding) {
    cpphttp::HttpServer server(8142);
    server.Get("/test", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("OK");
    });
    server.Start();

    cpptcpnet::TcpClient client;
    auto session = std::make_shared<TestClientSession>();
    client.SetDataHandler([session](uint64_t, const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(session->mutex);
        session->buffer.insert(session->buffer.end(), data.begin(), data.end());
        session->cv.notify_all();
    });

    cpppubsub::Worker worker;
    auto sub = client.GetEventBroker().Subscribe<cpptcpnet::ConnectionEvent>("state_events");
    worker.AddSubscription<cpptcpnet::ConnectionEvent>(sub, [&](const cpptcpnet::ConnectionEvent& e) {
        std::lock_guard<std::mutex> lock(session->mutex);
        if (e.state == cpptcpnet::ConnectionState::Connected) session->connected = true;
        else session->disconnected = true;
        session->cv.notify_all();
    });
    worker.Start();
    client.Start();

    uint64_t session_id = client.Connect("127.0.0.1", 8142);

    {
        std::unique_lock<std::mutex> lock(session->mutex);
        session->cv.wait_for(lock, std::chrono::seconds(5), [&]() { return session->connected; });
    }

    std::string smugg_req = 
        "POST /test HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: identity, chunked\r\n"
        "Content-Length: 5\r\n\r\n"
        "0\r\n\r\n";
    client.Send(session_id, smugg_req);

    {
        std::unique_lock<std::mutex> lock(session->mutex);
        session->cv.wait_for(lock, std::chrono::seconds(5), [&]() {
            std::string s(session->buffer.begin(), session->buffer.end());
            return s.find("HTTP/1.1 400 Bad Request") != std::string::npos;
        });
        std::string s(session->buffer.begin(), session->buffer.end());
        EXPECT_NE(s.find("HTTP/1.1 400 Bad Request"), std::string::npos);
    }

    client.Stop();
    worker.Stop();
    server.Stop();
}

TEST(HttpIntegrationTest, StaticDirTest) {
    namespace fs = std::filesystem;
    auto temp_dir = fs::temp_directory_path() / ("cpphttp_static_test_" + std::to_string(std::random_device{}()));
    fs::create_directories(temp_dir);
    fs::create_directories(temp_dir / "nested");

    // Create test files
    {
        std::ofstream out(temp_dir / "index.html");
        out << "<html><body>Hello HTML</body></html>";
    }
    {
        std::ofstream out(temp_dir / "style.css");
        out << "body { color: red; }";
    }
    {
        std::ofstream out(temp_dir / "data.json");
        out << "{\"key\": \"value\"}";
    }
    {
        std::ofstream out(temp_dir / "nested" / "file.txt");
        out << "Nested content";
    }

    cpphttp::HttpServer server(8145);
    server.StaticDir("/static", temp_dir.string());
    server.Start();

    cpphttp::HttpClient client("127.0.0.1", 8145);

    // 1. GET index.html
    auto res_html = client.SendRequest("GET", "/static/index.html");
    EXPECT_EQ(res_html.status_code, 200);
    EXPECT_EQ(res_html.body, "<html><body>Hello HTML</body></html>");
    EXPECT_EQ(res_html.headers.at("Content-Type"), "text/html");

    // 2. GET style.css
    auto res_css = client.SendRequest("GET", "/static/style.css");
    EXPECT_EQ(res_css.status_code, 200);
    EXPECT_EQ(res_css.body, "body { color: red; }");
    EXPECT_EQ(res_css.headers.at("Content-Type"), "text/css");

    // 3. GET data.json
    auto res_json = client.SendRequest("GET", "/static/data.json");
    EXPECT_EQ(res_json.status_code, 200);
    EXPECT_EQ(res_json.body, "{\"key\": \"value\"}");
    EXPECT_EQ(res_json.headers.at("Content-Type"), "application/json");

    // 4. GET nested/file.txt
    auto res_nested = client.SendRequest("GET", "/static/nested/file.txt");
    EXPECT_EQ(res_nested.status_code, 200);
    EXPECT_EQ(res_nested.body, "Nested content");
    EXPECT_EQ(res_nested.headers.at("Content-Type"), "text/plain");

    // 5. GET nonexistent file
    auto res_404 = client.SendRequest("GET", "/static/nonexistent.html");
    EXPECT_EQ(res_404.status_code, 404);

    // 6. GET directory itself should return 404 (nested has no index.html)
    auto res_dir = client.SendRequest("GET", "/static/nested/");
    EXPECT_EQ(res_dir.status_code, 404);

    // 7. GET root directory itself should return 200 with index.html
    auto res_root_dir = client.SendRequest("GET", "/static");
    EXPECT_EQ(res_root_dir.status_code, 200);
    EXPECT_EQ(res_root_dir.body, "<html><body>Hello HTML</body></html>");

    // 7.b GET root directory with trailing slash
    auto res_root_dir_slash = client.SendRequest("GET", "/static/");
    EXPECT_EQ(res_root_dir_slash.status_code, 200);
    EXPECT_EQ(res_root_dir_slash.body, "<html><body>Hello HTML</body></html>");

    // 8. HEAD request should be handled, returning correct headers but empty body
    auto res_head = client.SendRequest("HEAD", "/static/index.html", "", cpphttp::HeaderMap{{"Accept-Encoding", "identity"}});
    EXPECT_EQ(res_head.status_code, 200);
    EXPECT_EQ(res_head.body, "");
    EXPECT_EQ(res_head.headers.at("Content-Type"), "text/html");
    EXPECT_EQ(res_head.headers.at("Content-Length"), std::to_string(std::string("<html><body>Hello HTML</body></html>").size()));

    // 9. GET directory traversal attempt (should return 404 due to routing or path validation)
    auto res_traversal1 = client.SendRequest("GET", "/static/../index.html");
    EXPECT_EQ(res_traversal1.status_code, 404);

    // 10. GET directory traversal attempt using double encoding/slash bypass (should return 404)
    auto res_traversal2 = client.SendRequest("GET", "/static/%2e%2e%2findex.html");
    EXPECT_EQ(res_traversal2.status_code, 404);

    // 11. Large file streaming test (12MB file)
    {
        std::string large_file_path = (temp_dir / "large.bin").string();
        std::ofstream large_out(large_file_path, std::ios::binary);
        std::vector<char> dummy_chunk(1024 * 1024, 'A'); // 1MB chunk
        for (int i = 0; i < 12; ++i) {
            large_out.write(dummy_chunk.data(), dummy_chunk.size());
        }
        large_out.close();

        client.SetMaxBodySize(15 * 1024 * 1024);
        auto res_large = client.SendRequest("GET", "/static/large.bin");
        EXPECT_EQ(res_large.status_code, 200);
        EXPECT_EQ(res_large.headers.at("Content-Length"), std::to_string(12 * 1024 * 1024));
        EXPECT_EQ(res_large.body.size(), 12 * 1024 * 1024);
        
        bool correct_content = true;
        for (char c : res_large.body) {
            if (c != 'A') {
                correct_content = false;
                break;
            }
        }
        EXPECT_TRUE(correct_content);
    }

    server.Stop();

    // Cleanup
    fs::remove_all(temp_dir);
}

TEST(HttpIntegrationTest, StaticDirRangeRequestTest) {
    namespace fs = std::filesystem;
    auto temp_dir = fs::temp_directory_path() / ("cpphttp_static_range_test_" + std::to_string(std::random_device{}()));
    fs::create_directories(temp_dir);

    // Create test file with 26 bytes of alphabet
    std::string test_data = "abcdefghijklmnopqrstuvwxyz";
    {
        std::ofstream out(temp_dir / "alphabet.txt");
        out << test_data;
    }

    cpphttp::HttpServer server(8148);
    server.StaticDir("/static", temp_dir.string());
    server.Start();

    cpphttp::HttpClient client("127.0.0.1", 8148);

    // 1. Partial Range Request: bytes=0-4 (5 bytes: "abcde")
    {
        auto res = client.SendRequest("GET", "/static/alphabet.txt", "", cpphttp::HeaderMap{{"Range", "bytes=0-4"}});
        EXPECT_EQ(res.status_code, 206);
        EXPECT_EQ(res.body, "abcde");
        EXPECT_EQ(res.headers.at("Content-Length"), "5");
        EXPECT_EQ(res.headers.at("Content-Range"), "bytes 0-4/26");
        EXPECT_EQ(res.headers.at("Accept-Ranges"), "bytes");
    }

    // 2. Partial Range Request: bytes=10-15 (6 bytes: "klmnop")
    {
        auto res = client.SendRequest("GET", "/static/alphabet.txt", "", cpphttp::HeaderMap{{"Range", "bytes=10-15"}});
        EXPECT_EQ(res.status_code, 206);
        EXPECT_EQ(res.body, "klmnop");
        EXPECT_EQ(res.headers.at("Content-Length"), "6");
        EXPECT_EQ(res.headers.at("Content-Range"), "bytes 10-15/26");
    }

    // 3. Partial Range Request: bytes=20- (6 bytes: "uvwxyz")
    {
        auto res = client.SendRequest("GET", "/static/alphabet.txt", "", cpphttp::HeaderMap{{"Range", "bytes=20-"}});
        EXPECT_EQ(res.status_code, 206);
        EXPECT_EQ(res.body, "uvwxyz");
        EXPECT_EQ(res.headers.at("Content-Length"), "6");
        EXPECT_EQ(res.headers.at("Content-Range"), "bytes 20-25/26");
    }

    // 4. Partial Range Request: bytes=20-100 (6 bytes: "uvwxyz" - clipped to file_size - 1)
    {
        auto res = client.SendRequest("GET", "/static/alphabet.txt", "", cpphttp::HeaderMap{{"Range", "bytes=20-100"}});
        EXPECT_EQ(res.status_code, 206);
        EXPECT_EQ(res.body, "uvwxyz");
        EXPECT_EQ(res.headers.at("Content-Length"), "6");
        EXPECT_EQ(res.headers.at("Content-Range"), "bytes 20-25/26");
    }

    // 5. Invalid Range Request: bytes=30-40 (Out of bounds) -> 416
    {
        auto res = client.SendRequest("GET", "/static/alphabet.txt", "", cpphttp::HeaderMap{{"Range", "bytes=30-40"}});
        EXPECT_EQ(res.status_code, 416);
        EXPECT_EQ(res.headers.at("Content-Range"), "bytes */26");
    }

    // 6. Malformed Range Request (falls back to 200 OK)
    {
        auto res = client.SendRequest("GET", "/static/alphabet.txt", "", cpphttp::HeaderMap{{"Range", "bytes=abc-10"}});
        EXPECT_EQ(res.status_code, 200);
        EXPECT_EQ(res.body, test_data);
    }

    // 7. Suffix/Prefix malformed Range Request (falls back to 416 since start > end)
    {
        auto res = client.SendRequest("GET", "/static/alphabet.txt", "", cpphttp::HeaderMap{{"Range", "bytes=10-5"}});
        EXPECT_EQ(res.status_code, 416);
    }

    // 8. Suffix Range Request: bytes=-5 (5 bytes: "vwxyz")
    {
        auto res = client.SendRequest("GET", "/static/alphabet.txt", "", cpphttp::HeaderMap{{"Range", "bytes=-5"}});
        EXPECT_EQ(res.status_code, 206);
        EXPECT_EQ(res.body, "vwxyz");
        EXPECT_EQ(res.headers.at("Content-Length"), "5");
        EXPECT_EQ(res.headers.at("Content-Range"), "bytes 21-25/26");
    }

    // 9. Suffix Range Request: bytes=-100 (clipped to file size -> 26 bytes: alphabet)
    {
        auto res = client.SendRequest("GET", "/static/alphabet.txt", "", cpphttp::HeaderMap{{"Range", "bytes=-100"}});
        EXPECT_EQ(res.status_code, 206);
        EXPECT_EQ(res.body, test_data);
        EXPECT_EQ(res.headers.at("Content-Length"), "26");
        EXPECT_EQ(res.headers.at("Content-Range"), "bytes 0-25/26");
    }

    // 10. Suffix Range Request with length 0: bytes=-0 (invalid suffix length, fallback to 200 OK)
    {
        auto res = client.SendRequest("GET", "/static/alphabet.txt", "", cpphttp::HeaderMap{{"Range", "bytes=-0"}});
        EXPECT_EQ(res.status_code, 200);
        EXPECT_EQ(res.body, test_data);
    }

    server.Stop();
    fs::remove_all(temp_dir);
}


TEST(HttpIntegrationTest, StaticDirSpaModeTest) {
    namespace fs = std::filesystem;
    auto temp_dir = fs::temp_directory_path() / ("cpphttp_static_spa_test_" + std::to_string(std::random_device{}()));
    fs::create_directories(temp_dir);

    // Create test files
    {
        std::ofstream out(temp_dir / "index.html");
        out << "<html><body>SPA Root</body></html>";
    }
    {
        std::ofstream out(temp_dir / "style.css");
        out << "body { color: red; }";
    }

    cpphttp::HttpServer server(8199);
    server.StaticDir("/app", temp_dir.string(), true);
    server.Start();

    cpphttp::HttpClient client("127.0.0.1", 8199);

    // 1. GET existing file
    auto res_css = client.SendRequest("GET", "/app/style.css");
    EXPECT_EQ(res_css.status_code, 200);
    EXPECT_EQ(res_css.body, "body { color: red; }");

    // 2. GET unknown file (should fallback to index.html)
    auto res_unknown = client.SendRequest("GET", "/app/unknown.html");
    EXPECT_EQ(res_unknown.status_code, 200);
    EXPECT_EQ(res_unknown.body, "<html><body>SPA Root</body></html>");
    EXPECT_EQ(res_unknown.headers.at("Content-Type"), "text/html");

    // 3. GET unknown sub-route (should fallback to index.html)
    auto res_sub = client.SendRequest("GET", "/app/users/123");
    EXPECT_EQ(res_sub.status_code, 200);
    EXPECT_EQ(res_sub.body, "<html><body>SPA Root</body></html>");

    // 4. GET root directory
    auto res_root = client.SendRequest("GET", "/app/");
    EXPECT_EQ(res_root.status_code, 200);
    EXPECT_EQ(res_root.body, "<html><body>SPA Root</body></html>");

    // 5. POST unknown route (should fallback to index.html as well for SPA, or maybe 404?)
    // Note: Our StaticDir is registered as GET only, so POST should fail with 404
    auto res_post = client.SendRequest("POST", "/app/users");
    EXPECT_EQ(res_post.status_code, 404);

    server.Stop();
    fs::remove_all(temp_dir);
}

TEST(WebSocketIntegrationExtendedTest, ClientPayloadTooLarge) {
    cpphttp::HttpServer server(8143);
    
    cpphttp::WebSocketBehavior ws_behavior;
    ws_behavior.on_open = [](std::shared_ptr<cpphttp::WebSocketConnection> conn) {
        conn->Send("123456789012345");
    };
    server.WebSocket("/ws", std::move(ws_behavior));
    server.Start();

    cpphttp::WebSocketClient client;
    client.SetMaxBodySize(10);

    std::mutex ws_mtx;
    std::condition_variable ws_cv;
    uint16_t close_status = 0;
    bool closed = false;

    client.OnClose([&](uint16_t status, const std::string &reason) {
        std::lock_guard<std::mutex> lock(ws_mtx);
        close_status = status;
        closed = true;
        ws_cv.notify_all();
    });

    ASSERT_TRUE(client.Connect("ws://127.0.0.1:8143/ws"));

    {
        std::unique_lock<std::mutex> lock(ws_mtx);
        ws_cv.wait_for(lock, std::chrono::seconds(5), [&]() { return closed; });
        EXPECT_TRUE(closed);
        EXPECT_EQ(close_status, 1009); // Message too big
    }

    client.Close();
    server.Stop();
}

TEST(RateLimiterTest, BasicRateLimiting) {
    cpphttp::HttpServer server(8146);
    server.Use(cpphttp::RateLimiter(2, std::chrono::seconds(2))); // 2 requests per 2 seconds

    server.Get("/rate", [](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("OK");
    });
    server.Start();

    cpphttp::HttpClient client("127.0.0.1", 8146);

    auto res1 = client.SendRequest("GET", "/rate");
    EXPECT_EQ(res1.status_code, 200);

    auto res2 = client.SendRequest("GET", "/rate");
    EXPECT_EQ(res2.status_code, 200);

    auto res3 = client.SendRequest("GET", "/rate");
    EXPECT_EQ(res3.status_code, 429);
    EXPECT_EQ(res3.headers.at("Retry-After"), "2");

    std::this_thread::sleep_for(std::chrono::seconds(3));

    auto res4 = client.SendRequest("GET", "/rate");
    EXPECT_EQ(res4.status_code, 200);

    server.Stop();
}

TEST(ResponseCacheTest, BasicCaching) {
    cpphttp::HttpServer server(8147);
    cpphttp::ResponseCache cache(std::chrono::seconds(2));
    
    std::atomic<int> call_count{0};
    server.Get("/cache", cache.Wrap([&call_count](const cpphttp::HttpRequest &req) {
        call_count++;
        return cpphttp::HttpResponse::Plain("Call: " + std::to_string(call_count.load()));
    }));
    server.Start();

    cpphttp::HttpClient client("127.0.0.1", 8147);

    auto res1 = client.SendRequest("GET", "/cache");
    EXPECT_EQ(res1.status_code, 200);
    EXPECT_EQ(res1.body, "Call: 1");

    auto res2 = client.SendRequest("GET", "/cache");
    EXPECT_EQ(res2.status_code, 200);
    EXPECT_EQ(res2.body, "Call: 1"); // Should be cached

    std::this_thread::sleep_for(std::chrono::seconds(3));

    auto res3 = client.SendRequest("GET", "/cache");
    EXPECT_EQ(res3.status_code, 200);
    EXPECT_EQ(res3.body, "Call: 2"); // Cache expired

    server.Stop();
}

TEST(HttpStreamingTest, DownloadLargeFileStream) {
    // 1. Create a 5MB temporary file
    std::string temp_filename = "temp_download_5mb.bin";
    {
        std::ofstream outfile(temp_filename, std::ios::binary);
        std::string pattern = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        for (int i = 0; i < 80000; ++i) { // ~5MB
            outfile.write(pattern.data(), pattern.size());
        }
    }

    uint16_t port = 8150;
    cpphttp::HttpServer server(port);
    server.Get("/download", [temp_filename](const cpphttp::HttpRequest &req) {
        cpphttp::HttpResponse res;
        res.status_code = 200;
        res.status_message = "OK";
        res.is_file = true;
        res.file_path = temp_filename;
        res.headers["Content-Type"] = "application/octet-stream";
        res.headers["Content-Length"] = std::to_string(std::filesystem::file_size(temp_filename));
        return res;
    });
    server.Start();

    cpphttp::HttpClient client("127.0.0.1", port);
    size_t received_bytes = 0;
    size_t chunk_count = 0;

    auto on_chunk = [&](const cpphttp::HttpResponse &res, const std::string &chunk, bool is_final) {
        if (!is_final) {
            received_bytes += chunk.size();
            chunk_count++;
        } else {
            EXPECT_EQ(res.status_code, 200);
        }
    };

    cpphttp::HttpResponse final_res = client.GetStream("/download", on_chunk);
    EXPECT_EQ(final_res.status_code, 200);
    EXPECT_EQ(received_bytes, std::filesystem::file_size(temp_filename));
    EXPECT_GT(chunk_count, 0);

    server.Stop();
    std::filesystem::remove(temp_filename);
}

TEST(HttpStreamingTest, UploadLargeFileStreamAsync) {
    uint16_t port = 8151;
    cpphttp::HttpServer server(port);
    
    std::atomic<size_t> received_bytes{0};
    std::atomic<size_t> chunk_count{0};
    std::atomic<bool> final_called{false};

    server.PostStream("/upload", [&](const cpphttp::HttpRequest &req, const std::string &chunk, bool is_final) -> std::optional<cpphttp::HttpResponse> {
        if (!is_final) {
            received_bytes += chunk.size();
            chunk_count++;
            return std::nullopt;
        } else {
            final_called = true;
            return cpphttp::HttpResponse::Plain("Upload Complete! Received: " + std::to_string(received_bytes.load()));
        }
    });
    server.Start();

    cpphttp::HttpClient client("127.0.0.1", port);
    
    // Upload 5MB using PostStreamAsync
    size_t total_to_send = 5 * 1024 * 1024;
    size_t sent_bytes = 0;
    auto stream_provider = [&](size_t max_chunk) -> std::string {
        if (sent_bytes >= total_to_send) return "";
        size_t size = std::min(max_chunk, total_to_send - sent_bytes);
        std::string chunk(size, 'A');
        sent_bytes += size;
        return chunk;
    };

    std::future<cpphttp::HttpResponse> fut = client.PostStreamAsync("/upload", {}, stream_provider, total_to_send);
    cpphttp::HttpResponse res = fut.get();

    EXPECT_EQ(res.status_code, 200);
    EXPECT_EQ(res.body, "Upload Complete! Received: " + std::to_string(total_to_send));
    EXPECT_EQ(received_bytes.load(), total_to_send);
    EXPECT_GT(chunk_count.load(), 0);
    EXPECT_TRUE(final_called.load());

    server.Stop();
}

TEST(HttpStreamingTest, UploadLargeFileStreamChunkedAsync) {
    cpptcpnet::SetLogger([](cpptcpnet::LogSeverity severity, const std::string &className, const std::string &message) {
        std::cout << "[LOG][" << className << "] " << message << std::endl;
    });

    uint16_t port = 8152;
    cpphttp::HttpServer server(port);
    
    std::atomic<size_t> received_bytes{0};
    std::atomic<size_t> chunk_count{0};
    std::atomic<bool> final_called{false};

    server.PostStream("/upload_chunked", [&](const cpphttp::HttpRequest &req, const std::string &chunk, bool is_final) -> std::optional<cpphttp::HttpResponse> {
        std::cout << "[Server] Chunk received: size=" << chunk.size() << ", is_final=" << is_final << std::endl;
        if (!is_final) {
            received_bytes += chunk.size();
            chunk_count++;
            return std::nullopt;
        } else {
            final_called = true;
            return cpphttp::HttpResponse::Plain("Chunked Complete! Received: " + std::to_string(received_bytes.load()));
        }
    });
    server.Start();

    cpphttp::HttpClient client("127.0.0.1", port);
    
    // Chunked Upload: stream provider returns 5 chunks of 1MB, then empty
    size_t total_chunks = 5;
    size_t current_chunk = 0;
    auto stream_provider = [&](size_t max_chunk) -> std::string {
        if (current_chunk >= total_chunks) return "";
        current_chunk++;
        std::cout << "[Client] Providing chunk " << current_chunk << " of size 1MB" << std::endl;
        return std::string(1024 * 1024, 'B'); // 1MB chunk
    };

    std::unordered_map<std::string, std::string> headers = {{"Transfer-Encoding", "chunked"}};
    std::cout << "[Client] Starting PostStreamAsync..." << std::endl;
    std::future<cpphttp::HttpResponse> fut = client.PostStreamAsync("/upload_chunked", headers, stream_provider, 0);
    cpphttp::HttpResponse res = fut.get();
    std::cout << "[Client] PostStreamAsync completed with status=" << res.status_code << ", body=" << res.body << std::endl;

    EXPECT_EQ(res.status_code, 200);
    EXPECT_EQ(res.body, "Chunked Complete! Received: 5242880");
    EXPECT_EQ(received_bytes.load(), 5242880);
    EXPECT_EQ(chunk_count.load(), 5);
    EXPECT_TRUE(final_called.load());

    server.Stop();
    cpptcpnet::SetLogger(nullptr);
}

TEST(HttpIntegrationTest, EmptyBodyContentLengthZero) {
    cpphttp::HttpServer server(8153);
    std::string post_content_length;
    std::string get_content_length;
    
    server.Post("/test", [&](const cpphttp::HttpRequest &req) {
        post_content_length = req.GetHeader("Content-Length");
        return cpphttp::HttpResponse::Plain("OK");
    });
    
    server.Get("/test", [&](const cpphttp::HttpRequest &req) {
        get_content_length = req.GetHeader("Content-Length");
        return cpphttp::HttpResponse::Plain("OK");
    });
    
    server.Start();
    
    cpphttp::HttpClient client("127.0.0.1", 8153);
    auto res1 = client.Post("/test", "");
    EXPECT_EQ(res1.status_code, 200);
    EXPECT_EQ(post_content_length, "0");
    
    auto res2 = client.Get("/test");
    EXPECT_EQ(res2.status_code, 200);
    EXPECT_EQ(get_content_length, "");
    
    server.Stop();
}

TEST(HttpStreamingTest, HttpClientDestructorCancellation) {
    uint16_t port = 8154;
    cpphttp::HttpServer server(port);
    
    server.PostStream("/upload", [&](const cpphttp::HttpRequest &req, const std::string &chunk, bool is_final) -> std::optional<cpphttp::HttpResponse> {
        if (is_final) {
            return cpphttp::HttpResponse::Plain("Upload Complete");
        }
        return std::nullopt;
    });
    server.Start();

    auto start_time = std::chrono::steady_clock::now();
    {
        cpphttp::HttpClient client("127.0.0.1", port);
        
        // A stream provider that runs forever
        auto stream_provider = [&](size_t max_chunk) -> std::string {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return std::string(1024, 'A');
        };

        std::future<cpphttp::HttpResponse> fut = client.PostStreamAsync("/upload", {}, stream_provider, 1024 * 1024);
        
        // Wait a small moment to ensure the upload thread is active
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Destructor of client will be called here at the end of block.
        // It should abort the upload thread and return immediately.
    }
    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    // The total upload would have taken ~50 seconds (1000 chunks of 1KB with 50ms sleep).
    // The cancellation should make the destructor return very fast (e.g. within 1.5 seconds).
    EXPECT_LT(elapsed, 1500);

    server.Stop();
}

TEST(HttpStreamingTest, UploadStreamChunkedEarlyAbort) {
    uint16_t port = 8158;
    cpphttp::HttpServer server(port);
    
    std::atomic<size_t> chunk_count{0};
    std::atomic<bool> final_called{false};

    server.PostStream("/upload_abort_chunked", [&](const cpphttp::HttpRequest &req, const std::string &chunk, bool is_final) -> std::optional<cpphttp::HttpResponse> {
        if (!is_final) {
            chunk_count++;
            if (chunk_count == 1) {
                // Abort early on the first chunk
                return cpphttp::HttpResponse::Plain("Early Abort", 400);
            }
            return std::nullopt;
        } else {
            final_called = true;
            return cpphttp::HttpResponse::Plain("Complete");
        }
    });
    server.Start();

    cpphttp::HttpClient client("127.0.0.1", port);
    
    size_t provider_calls = 0;
    auto stream_provider = [&](size_t max_chunk) -> std::string {
        provider_calls++;
        if (provider_calls == 1) return "FirstChunk";
        if (provider_calls == 2) return "SecondChunk";
        return "";
    };

    std::unordered_map<std::string, std::string> headers = {{"Transfer-Encoding", "chunked"}};
    std::future<cpphttp::HttpResponse> fut = client.PostStreamAsync("/upload_abort_chunked", headers, stream_provider, 0);
    cpphttp::HttpResponse res = fut.get();

    EXPECT_EQ(res.status_code, 400);
    EXPECT_EQ(res.body, "Early Abort");
    EXPECT_EQ(chunk_count.load(), 1); // Should only process the first chunk!
    EXPECT_FALSE(final_called.load()); // Should not have completed normal execution

    server.Stop();
}

TEST(HttpIntegrationTest, CorsMiddleware) {
    cpphttp::HttpServer server(8155);

    cpphttp::CorsConfig cors_config;
    cors_config.allow_origin = "https://example.com";
    cors_config.allow_credentials = true;
    cors_config.expose_headers = {"X-Custom-Header"};
    
    server.Use(cpphttp::Cors(cors_config));

    server.Get("/cors-test", [&](const cpphttp::HttpRequest &req) {
        auto res = cpphttp::HttpResponse::Plain("CORS OK");
        res.headers["X-Custom-Header"] = "foo";
        return res;
    });

    server.Start();

    cpphttp::HttpClient client("127.0.0.1", 8155);

    // Test Preflight
    try {
        std::unordered_map<std::string, std::string> headers;
        auto res = client.SendRequest("OPTIONS", "/cors-test", "", headers);
        EXPECT_EQ(res.status_code, 204);
        EXPECT_EQ(res.headers.at("Access-Control-Allow-Origin"), "https://example.com");
        EXPECT_EQ(res.headers.at("Access-Control-Allow-Credentials"), "true");
        EXPECT_NE(res.headers.at("Access-Control-Allow-Methods").find("GET"), std::string::npos);
        EXPECT_EQ(res.headers.at("Access-Control-Max-Age"), "86400");
    } catch (const std::exception &e) {
        FAIL() << "OPTIONS Request failed: " << e.what();
    }

    // Test Actual Request
    try {
        auto res = client.Get("/cors-test");
        EXPECT_EQ(res.status_code, 200);
        EXPECT_EQ(res.headers.at("Access-Control-Allow-Origin"), "https://example.com");
        EXPECT_EQ(res.headers.at("Access-Control-Allow-Credentials"), "true");
        EXPECT_EQ(res.headers.at("Access-Control-Expose-Headers"), "X-Custom-Header");
        EXPECT_EQ(res.body, "CORS OK");
    } catch (const std::exception &e) {
        FAIL() << "GET Request failed: " << e.what();
    }

    server.Stop();
}

TEST(HttpIntegrationTest, CorsMiddlewareScoped) {
    cpphttp::HttpServer server(8156);

    cpphttp::CorsConfig cors_config;
    cors_config.allow_origin = "https://specific-endpoint.com";
    
    // Apply CORS only to the /api prefix
    server.Use("/api", cpphttp::Cors(cors_config));

    server.Get("/api/data", [&](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("API Data");
    });

    server.Get("/public/data", [&](const cpphttp::HttpRequest &req) {
        return cpphttp::HttpResponse::Plain("Public Data");
    });

    server.Start();

    cpphttp::HttpClient client("127.0.0.1", 8156);

    // Test Scoped Endpoint (Preflight)
    try {
        std::unordered_map<std::string, std::string> headers;
        auto res = client.SendRequest("OPTIONS", "/api/data", "", headers);
        EXPECT_EQ(res.status_code, 204);
        EXPECT_EQ(res.headers.at("Access-Control-Allow-Origin"), "https://specific-endpoint.com");
    } catch (const std::exception &e) {
        FAIL() << "Scoped OPTIONS Request failed: " << e.what();
    }

    // Test Scoped Endpoint (Actual Request)
    try {
        auto res = client.Get("/api/data");
        EXPECT_EQ(res.status_code, 200);
        EXPECT_EQ(res.headers.at("Access-Control-Allow-Origin"), "https://specific-endpoint.com");
        EXPECT_EQ(res.body, "API Data");
    } catch (const std::exception &e) {
        FAIL() << "Scoped GET Request failed: " << e.what();
    }

    // Test Unscoped Endpoint (Preflight) -> should fail (method not allowed / handled by default)
    try {
        std::unordered_map<std::string, std::string> headers;
        auto res = client.SendRequest("OPTIONS", "/public/data", "", headers);
        // Since no route handles OPTIONS for /public/data and no middleware intercepts it, it will return 405 Method Not Allowed or 404
        EXPECT_EQ(res.headers.count("Access-Control-Allow-Origin"), 0);
        EXPECT_NE(res.status_code, 204);
    } catch (const std::exception &e) {
        FAIL() << "Unscoped OPTIONS Request failed: " << e.what();
    }

    // Test Unscoped Endpoint (Actual Request)
    try {
        auto res = client.Get("/public/data");
        EXPECT_EQ(res.status_code, 200);
        EXPECT_EQ(res.headers.count("Access-Control-Allow-Origin"), 0);
        EXPECT_EQ(res.body, "Public Data");
    } catch (const std::exception &e) {
        FAIL() << "Unscoped GET Request failed: " << e.what();
    }

    server.Stop();
}
