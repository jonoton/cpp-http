#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <cstring>
#include <openssl/evp.h>

#include <optional>
#include <random>
#include <zlib.h>

#include "cppasyncworker.hpp"
#include "cpptcpnet.hpp"

namespace cpphttp {

constexpr int VERSION_MAJOR = 1;
constexpr int VERSION_MINOR = 2;
constexpr int VERSION_PATCH = 0;

/**
 * @brief Returns the library version as a string.
 * @return A reference to the version string in "MAJOR.MINOR.PATCH" format.
 */
inline const std::string &version() {
  static const std::string version_str = []() {
    return std::to_string(VERSION_MAJOR) + "." + std::to_string(VERSION_MINOR) +
           "." + std::to_string(VERSION_PATCH);
  }();
  return version_str;
}

inline char safe_tolower(char c) {
  return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

struct CaseInsensitiveHash {
  size_t operator()(const std::string &str) const {
    size_t h = 0;
    for (char c : str) {
      h = h * 31 + static_cast<unsigned char>(safe_tolower(c));
    }
    return h;
  }
};

struct CaseInsensitiveEqual {
  bool operator()(const std::string &lhs, const std::string &rhs) const {
    if (lhs.size() != rhs.size())
      return false;
    for (size_t i = 0; i < lhs.size(); ++i) {
      if (safe_tolower(lhs[i]) != safe_tolower(rhs[i])) {
        return false;
      }
    }
    return true;
  }
};

using HeaderMap = std::unordered_map<std::string, std::string,
                                     CaseInsensitiveHash, CaseInsensitiveEqual>;
using MultiHeaderMap =
    std::unordered_multimap<std::string, std::string, CaseInsensitiveHash,
                            CaseInsensitiveEqual>;

inline bool is_valid_header_value(const std::string &val) {
  return val.find('\r') == std::string::npos &&
         val.find('\n') == std::string::npos;
}

inline std::optional<std::string> gzip_compress(const std::string &data) {
  if (data.size() > std::numeric_limits<uInt>::max()) {
    return std::nullopt;
  }
  z_stream zs;
  std::memset(&zs, 0, sizeof(zs));
  if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8,
                   Z_DEFAULT_STRATEGY) != Z_OK) {
    return std::nullopt;
  }
  zs.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(data.data()));
  zs.avail_in = static_cast<uInt>(data.size());

  int ret;
  char outbuffer[32768];
  std::string outstring;

  do {
    zs.next_out = reinterpret_cast<Bytef *>(outbuffer);
    zs.avail_out = sizeof(outbuffer);

    ret = deflate(&zs, Z_FINISH);

    if (outstring.size() < zs.total_out) {
      outstring.append(outbuffer, zs.total_out - outstring.size());
    }
  } while (ret == Z_OK);

  deflateEnd(&zs);

  if (ret != Z_STREAM_END) {
    return std::nullopt;
  }
  return outstring;
}

inline std::optional<std::string> gzip_decompress(const std::string &data) {
  if (data.size() > std::numeric_limits<uInt>::max()) {
    return std::nullopt;
  }
  z_stream zs;
  std::memset(&zs, 0, sizeof(zs));

  if (inflateInit2(&zs, 15 + 32) != Z_OK) {
    return std::nullopt;
  }

  zs.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(data.data()));
  zs.avail_in = static_cast<uInt>(data.size());

  int ret;
  char outbuffer[32768];
  std::string outstring;

  do {
    zs.next_out = reinterpret_cast<Bytef *>(outbuffer);
    zs.avail_out = sizeof(outbuffer);

    ret = inflate(&zs, Z_NO_FLUSH);

    if (outstring.size() < zs.total_out) {
      outstring.append(outbuffer, zs.total_out - outstring.size());
    }
  } while (ret == Z_OK);

  inflateEnd(&zs);

  if (ret != Z_STREAM_END) {
    return std::nullopt;
  }
  return outstring;
}

inline bool parse_url(const std::string &url, std::string &protocol,
                      std::string &host, uint16_t &port, std::string &path) {
  size_t proto_end = url.find("://");
  std::string host_port_path;
  if (proto_end != std::string::npos) {
    protocol = url.substr(0, proto_end);
    host_port_path = url.substr(proto_end + 3);
  } else {
    protocol = "http";
    host_port_path = url;
  }

  size_t path_start = host_port_path.find('/');
  if (path_start != std::string::npos) {
    path = host_port_path.substr(path_start);
    host_port_path = host_port_path.substr(0, path_start);
  } else {
    path = "/";
  }

  if (!host_port_path.empty() && host_port_path[0] == '[') {
    size_t bracket_end = host_port_path.find(']');
    if (bracket_end != std::string::npos) {
      host = host_port_path.substr(1, bracket_end - 1);
      if (host.empty()) {
        return false;
      }
      std::string remaining = host_port_path.substr(bracket_end + 1);
      if (!remaining.empty() && remaining[0] == ':') {
        try {
          unsigned long p = std::stoul(remaining.substr(1));
          if (p > 65535) {
            return false;
          }
          port = static_cast<uint16_t>(p);
        } catch (const std::exception &) {
          return false;
        }
      } else {
        port = (protocol == "https" || protocol == "wss") ? 443 : 80;
      }
      size_t frag_start = path.find('#');
      if (frag_start != std::string::npos) {
        path = path.substr(0, frag_start);
      }
      return true;
    }
  }

  size_t port_start = host_port_path.find(':');
  if (port_start != std::string::npos) {
    host = host_port_path.substr(0, port_start);
    try {
      unsigned long p = std::stoul(host_port_path.substr(port_start + 1));
      if (p > 65535) {
        return false;
      }
      port = static_cast<uint16_t>(p);
    } catch (const std::exception &) {
      return false;
    }
  } else {
    host = host_port_path;
    port = (protocol == "https" || protocol == "wss") ? 443 : 80;
  }

  if (host.empty()) {
    return false;
  }

  size_t frag_start = path.find('#');
  if (frag_start != std::string::npos) {
    path = path.substr(0, frag_start);
  }
  return true;
}

struct MultipartPart {
  std::string name;
  std::string filename;
  std::string content_type;
  std::string data;
};

inline std::vector<MultipartPart> ParseMultipart(const std::string &body,
                                                 const std::string &boundary) {
  std::vector<MultipartPart> parts;
  if (boundary.empty())
    return parts;

  std::string delimiter = "--" + boundary;
  std::string end_delimiter = delimiter + "--";

  size_t pos = 0;
  while (true) {
    pos = body.find(delimiter, pos);
    if (pos == std::string::npos)
      break;

    bool valid_start = false;
    if (pos == 0) {
      valid_start = true;
    } else if (body[pos - 1] == '\n') {
      valid_start = true;
    }

    if (!valid_start) {
      pos += delimiter.length();
      continue;
    }

    if (body.compare(pos, end_delimiter.length(), end_delimiter) == 0) {
      break;
    }

    size_t part_start = pos + delimiter.length();
    if (part_start + 2 <= body.length() && body[part_start] == '\r' &&
        body[part_start + 1] == '\n') {
      part_start += 2;
    } else if (part_start + 1 <= body.length() && body[part_start] == '\n') {
      part_start += 1;
    }

    size_t next_delim = part_start;
    while (true) {
      next_delim = body.find(delimiter, next_delim);
      if (next_delim == std::string::npos)
        break;
      if (next_delim > 0 && body[next_delim - 1] == '\n') {
        break;
      }
      next_delim += delimiter.length();
    }

    if (next_delim == std::string::npos)
      break;

    std::string part_raw = body.substr(part_start, next_delim - part_start);
    if (!part_raw.empty() && part_raw.back() == '\n') {
      part_raw.pop_back();
      if (!part_raw.empty() && part_raw.back() == '\r') {
        part_raw.pop_back();
      }
    }

    size_t header_end = part_raw.find("\r\n\r\n");
    size_t header_len = 4;
    if (header_end == std::string::npos) {
      header_end = part_raw.find("\n\n");
      header_len = 2;
    }

    if (header_end != std::string::npos) {
      std::string headers_str = part_raw.substr(0, header_end);
      std::string part_body = part_raw.substr(header_end + header_len);

      MultipartPart part;
      part.data = part_body;

      std::istringstream stream(headers_str);
      std::string line;
      while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r')
          line.pop_back();
        if (line.empty())
          continue;

        size_t colon = line.find(':');
        if (colon != std::string::npos) {
          std::string key = line.substr(0, colon);
          std::string val = line.substr(colon + 1);

          size_t first = val.find_first_not_of(" \t");
          size_t last = val.find_last_not_of(" \t\r\n");
          if (first != std::string::npos && last != std::string::npos) {
            val = val.substr(first, last - first + 1);
          }

          std::string lower_key = key;
          std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(),
                         safe_tolower);

          if (lower_key == "content-disposition") {
            size_t name_pos = val.find("name=\"");
            if (name_pos != std::string::npos) {
              size_t start = name_pos + 6;
              size_t end = val.find("\"", start);
              if (end != std::string::npos) {
                part.name = val.substr(start, end - start);
              }
            }
            size_t file_pos = val.find("filename=\"");
            if (file_pos != std::string::npos) {
              size_t start = file_pos + 10;
              size_t end = val.find("\"", start);
              if (end != std::string::npos) {
                part.filename = val.substr(start, end - start);
              }
            }
          } else if (lower_key == "content-type") {
            part.content_type = val;
          }
        }
      }
      parts.push_back(part);
    }

    pos = next_delim;
  }

  return parts;
}

// Helper function to encode to base64
inline std::string base64_encode(const std::string &in) {
  std::string out;
  const char lookup[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t i = 0;
  while (i + 3 <= in.size()) {
    uint32_t val = (static_cast<uint8_t>(in[i]) << 16) |
                   (static_cast<uint8_t>(in[i + 1]) << 8) |
                   static_cast<uint8_t>(in[i + 2]);
    out.push_back(lookup[(val >> 18) & 0x3F]);
    out.push_back(lookup[(val >> 12) & 0x3F]);
    out.push_back(lookup[(val >> 6) & 0x3F]);
    out.push_back(lookup[val & 0x3F]);
    i += 3;
  }
  if (i < in.size()) {
    size_t remaining = in.size() - i;
    uint32_t val = static_cast<uint8_t>(in[i]) << 16;
    if (remaining == 2) {
      val |= static_cast<uint8_t>(in[i + 1]) << 8;
    }
    out.push_back(lookup[(val >> 18) & 0x3F]);
    out.push_back(lookup[(val >> 12) & 0x3F]);
    if (remaining == 2) {
      out.push_back(lookup[(val >> 6) & 0x3F]);
      out.push_back('=');
    } else {
      out.push_back('=');
      out.push_back('=');
    }
  }
  return out;
}

// Compute standard Sec-WebSocket-Accept key
inline std::string ComputeAcceptKey(const std::string &ws_key) {
  std::string concatenated = ws_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int hash_len = 0;
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  if (!ctx) {
    return "";
  }
  if (EVP_DigestInit_ex(ctx, EVP_sha1(), nullptr) != 1 ||
      EVP_DigestUpdate(ctx, concatenated.c_str(), concatenated.length()) != 1 ||
      EVP_DigestFinal_ex(ctx, hash, &hash_len) != 1) {
    EVP_MD_CTX_free(ctx);
    return "";
  }
  EVP_MD_CTX_free(ctx);
  return base64_encode(std::string(reinterpret_cast<char *>(hash), hash_len));
}

// Helper function to decode URL percent-encoded characters
inline std::string url_decode(const std::string &str, bool decode_plus = true) {
  std::string decoded;
  decoded.reserve(str.size());
  for (size_t i = 0; i < str.size(); ++i) {
    if (str[i] == '%') {
      if (i + 2 < str.size()) {
        char hex[3] = {str[i + 1], str[i + 2], '\0'};
        char *endptr;
        long val = std::strtol(hex, &endptr, 16);
        if (endptr == hex + 2) {
          decoded += static_cast<char>(val);
          i += 2;
          continue;
        }
      }
    } else if (str[i] == '+' && decode_plus) {
      decoded += ' ';
      continue;
    }
    decoded += str[i];
  }
  return decoded;
}

// Split path into segments
inline std::vector<std::string> split_path(const std::string &path) {
  std::vector<std::string> segments;
  std::istringstream stream(path);
  std::string segment;
  while (std::getline(stream, segment, '/')) {
    if (!segment.empty()) {
      std::string decoded = url_decode(segment, false);
      if (decoded == ".") {
        continue;
      } else if (decoded == "..") {
        if (!segments.empty()) {
          segments.pop_back();
        }
      } else {
        segments.push_back(decoded);
      }
    }
  }
  return segments;
}

// Helper to parse chunked transfer encoding body incrementally to avoid O(N^2)
// complexity
template <typename Container>
inline bool ParseChunkedBodyIncrementalImpl(
    const Container &buffer, size_t &parsed_up_to, std::string &body,
    bool &bad_request, size_t max_body_size, bool &payload_too_large,
    std::function<void(const std::string &)> on_chunk = nullptr) {
  bad_request = false;
  payload_too_large = false;
  size_t idx = parsed_up_to;

  while (true) {
    if (idx >= buffer.size())
      return false;

    auto it_crlf = std::search(buffer.begin() + idx, buffer.end(),
                               reinterpret_cast<const uint8_t *>("\r\n"),
                               reinterpret_cast<const uint8_t *>("\r\n") + 2);
    if (it_crlf == buffer.end())
      return false;

    std::string size_str(buffer.begin() + idx, it_crlf);
    size_t chunk_size = 0;
    try {
      chunk_size = std::stoul(size_str, nullptr, 16);
    } catch (const std::exception &e) {
      bad_request = true;
      return false;
    }

    size_t chunk_data_start = std::distance(buffer.begin(), it_crlf) + 2;
    if (chunk_size == 0) {
      if (chunk_data_start + 2 <= buffer.size() &&
          buffer[chunk_data_start] == '\r' &&
          buffer[chunk_data_start + 1] == '\n') {
        parsed_up_to = chunk_data_start + 2;
        return true;
      }
      auto double_crlf =
          std::search(buffer.begin() + chunk_data_start, buffer.end(),
                      reinterpret_cast<const uint8_t *>("\r\n\r\n"),
                      reinterpret_cast<const uint8_t *>("\r\n\r\n") + 4);
      if (double_crlf == buffer.end())
        return false;
      parsed_up_to = std::distance(buffer.begin(), double_crlf) + 4;
      return true;
    }

    if (chunk_size > max_body_size ||
        (!on_chunk && body.size() + chunk_size > max_body_size)) {
      payload_too_large = true;
      return false;
    }

    if (buffer.size() - chunk_data_start < chunk_size + 2) {
      return false;
    }

    if (buffer[chunk_data_start + chunk_size] != '\r' ||
        buffer[chunk_data_start + chunk_size + 1] != '\n') {
      bad_request = true;
      return false;
    }

    if (on_chunk) {
      on_chunk(std::string(buffer.begin() + chunk_data_start,
                           buffer.begin() + chunk_data_start + chunk_size));
    } else {
      body.append(buffer.begin() + chunk_data_start,
                  buffer.begin() + chunk_data_start + chunk_size);
    }
    idx = chunk_data_start + chunk_size + 2;
    parsed_up_to = idx;
  }
}

inline bool ParseChunkedBodyIncremental(
    const std::vector<uint8_t> &buffer, size_t &parsed_up_to, std::string &body,
    bool &bad_request, size_t max_body_size, bool &payload_too_large,
    std::function<void(const std::string &)> on_chunk = nullptr) {
  return ParseChunkedBodyIncrementalImpl(buffer, parsed_up_to, body,
                                         bad_request, max_body_size,
                                         payload_too_large, on_chunk);
}

inline bool ParseChunkedBodyIncremental(
    const std::deque<uint8_t> &buffer, size_t &parsed_up_to, std::string &body,
    bool &bad_request, size_t max_body_size, bool &payload_too_large,
    std::function<void(const std::string &)> on_chunk = nullptr) {
  return ParseChunkedBodyIncrementalImpl(buffer, parsed_up_to, body,
                                         bad_request, max_body_size,
                                         payload_too_large, on_chunk);
}

template <typename Container>
inline bool ParseChunkedBodyImpl(const Container &buffer, size_t body_start_idx,
                                 std::string &body, size_t &consumed_bytes,
                                 bool &bad_request, size_t max_body_size,
                                 bool &payload_too_large) {
  body.clear();
  size_t parsed_up_to = body_start_idx;
  bool completed =
      ParseChunkedBodyIncremental(buffer, parsed_up_to, body, bad_request,
                                  max_body_size, payload_too_large);
  if (completed) {
    consumed_bytes = parsed_up_to - body_start_idx;
  }
  return completed;
}

inline bool ParseChunkedBody(const std::vector<uint8_t> &buffer,
                             size_t body_start_idx, std::string &body,
                             size_t &consumed_bytes, bool &bad_request,
                             size_t max_body_size, bool &payload_too_large) {
  return ParseChunkedBodyImpl(buffer, body_start_idx, body, consumed_bytes,
                              bad_request, max_body_size, payload_too_large);
}

inline bool ParseChunkedBody(const std::deque<uint8_t> &buffer,
                             size_t body_start_idx, std::string &body,
                             size_t &consumed_bytes, bool &bad_request,
                             size_t max_body_size, bool &payload_too_large) {
  return ParseChunkedBodyImpl(buffer, body_start_idx, body, consumed_bytes,
                              bad_request, max_body_size, payload_too_large);
}

// Match route pattern against requested segments
inline bool
match_route(const std::string &request_method,
            const std::vector<std::string> &request_segments,
            const std::string &route_method,
            const std::vector<std::string> &route_segments,
            std::unordered_map<std::string, std::string> &path_params) {
  if (request_method != route_method) {
    return false;
  }

  path_params.clear();

  for (size_t i = 0; i < route_segments.size(); ++i) {
    const std::string &route_seg = route_segments[i];

    if (route_seg == "*") {
      if (i + 1 == route_segments.size()) {
        std::string remaining;
        for (size_t j = i; j < request_segments.size(); ++j) {
          if (!remaining.empty())
            remaining += "/";
          remaining += request_segments[j];
        }
        path_params["*"] = remaining;
        return true;
      } else {
        if (i >= request_segments.size()) {
          return false;
        }
        path_params["*"] = request_segments[i];
        continue;
      }
    }

    if (i >= request_segments.size()) {
      return false;
    }

    if (!route_seg.empty() && route_seg[0] == ':') {
      path_params[route_seg.substr(1)] = request_segments[i];
    } else if (route_seg != request_segments[i]) {
      return false;
    }
  }

  return request_segments.size() == route_segments.size();
}

// WebSocket Frame Structure
struct WsFrame {
  bool fin = true;
  uint8_t opcode = 0;
  bool masked = false;
  uint8_t mask_key[4] = {0, 0, 0, 0};
  std::vector<uint8_t> payload;
};

// Parse a single WebSocket frame from a buffer.
template <typename Container>
inline bool ParseWsFrameImpl(Container &buffer, WsFrame &frame,
                             bool &protocol_error, bool &payload_too_big,
                             bool is_server_side = true,
                             size_t max_payload_size = 10 * 1024 * 1024) {
  protocol_error = false;
  payload_too_big = false;
  if (buffer.size() < 2) {
    return false;
  }

  uint8_t byte0 = buffer[0];
  uint8_t byte1 = buffer[1];

  frame.fin = (byte0 & 0x80) != 0;
  if ((byte0 & 0x70) != 0) {
    protocol_error = true;
    return false;
  }

  frame.opcode = byte0 & 0x0F;
  if (frame.opcode != 0x00 && frame.opcode != 0x01 && frame.opcode != 0x02 &&
      frame.opcode != 0x08 && frame.opcode != 0x09 && frame.opcode != 0x0A) {
    protocol_error = true;
    return false;
  }
  frame.masked = (byte1 & 0x80) != 0;

  if (is_server_side && !frame.masked) {
    protocol_error = true;
    return false;
  }
  if (!is_server_side && frame.masked) {
    protocol_error = true;
    return false;
  }

  uint64_t payload_len = byte1 & 0x7F;
  size_t header_len = 2;

  if (payload_len == 126) {
    if (buffer.size() < 4)
      return false;
    payload_len = (static_cast<uint64_t>(buffer[2]) << 8) | buffer[3];
    header_len = 4;
  } else if (payload_len == 127) {
    if (buffer.size() < 10)
      return false;
    payload_len = 0;
    for (int i = 0; i < 8; ++i) {
      payload_len = (payload_len << 8) | buffer[2 + i];
    }
    header_len = 10;
  }

  if (payload_len > max_payload_size) {
    payload_too_big = true;
    return false;
  }

  if (frame.opcode >= 0x08) {
    if (!frame.fin || payload_len > 125) {
      protocol_error = true;
      return false;
    }
  }

  if (frame.masked) {
    if (buffer.size() < header_len + 4)
      return false;
    std::copy(buffer.begin() + header_len, buffer.begin() + header_len + 4,
              frame.mask_key);
    header_len += 4;
  }

  if (buffer.size() < header_len + payload_len) {
    return false;
  }

  frame.payload.assign(buffer.begin() + header_len,
                       buffer.begin() + header_len + payload_len);

  if (frame.masked) {
    for (size_t i = 0; i < frame.payload.size(); ++i) {
      frame.payload[i] ^= frame.mask_key[i % 4];
    }
  }

  buffer.erase(buffer.begin(), buffer.begin() + header_len + payload_len);
  return true;
}

inline bool ParseWsFrame(std::vector<uint8_t> &buffer, WsFrame &frame,
                         bool &protocol_error, bool &payload_too_big,
                         bool is_server_side = true,
                         size_t max_payload_size = 10 * 1024 * 1024) {
  return ParseWsFrameImpl(buffer, frame, protocol_error, payload_too_big,
                          is_server_side, max_payload_size);
}

inline bool ParseWsFrame(std::deque<uint8_t> &buffer, WsFrame &frame,
                         bool &protocol_error, bool &payload_too_big,
                         bool is_server_side = true,
                         size_t max_payload_size = 10 * 1024 * 1024) {
  return ParseWsFrameImpl(buffer, frame, protocol_error, payload_too_big,
                          is_server_side, max_payload_size);
}

inline bool ParseWsFrame(std::vector<uint8_t> &buffer, WsFrame &frame,
                         bool &protocol_error, bool is_server_side = true) {
  bool payload_too_big = false;
  return ParseWsFrameImpl(buffer, frame, protocol_error, payload_too_big,
                          is_server_side);
}

inline bool ParseWsFrame(std::deque<uint8_t> &buffer, WsFrame &frame,
                         bool &protocol_error, bool is_server_side = true) {
  bool payload_too_big = false;
  return ParseWsFrameImpl(buffer, frame, protocol_error, payload_too_big,
                          is_server_side);
}

// Create a server-to-client WebSocket frame
inline std::vector<uint8_t> CreateWsFrame(uint8_t opcode,
                                          const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> frame;
  frame.push_back(0x80 | (opcode & 0x0F));

  uint64_t len = payload.size();
  if (len <= 125) {
    frame.push_back(static_cast<uint8_t>(len));
  } else if (len <= 65535) {
    frame.push_back(126);
    frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
    frame.push_back(static_cast<uint8_t>(len & 0xFF));
  } else {
    frame.push_back(127);
    for (int i = 7; i >= 0; --i) {
      frame.push_back(static_cast<uint8_t>((len >> (i * 8)) & 0xFF));
    }
  }

  frame.insert(frame.end(), payload.begin(), payload.end());
  return frame;
}

inline std::vector<uint8_t> CreateWsFrame(uint8_t opcode,
                                          const std::string &payload) {
  return CreateWsFrame(opcode,
                       std::vector<uint8_t>(payload.begin(), payload.end()));
}

class WebSocketConnection {
public:
  WebSocketConnection(uint64_t session_id,
                      std::shared_ptr<cpptcpnet::TcpListener> listener)
      : session_id_(session_id), listener_(listener) {}

  uint64_t GetSessionId() const { return session_id_; }

  bool Send(const std::string &message) {
    auto frame = CreateWsFrame(0x01, message);
    if (auto l = listener_.lock()) {
      return l->Send(session_id_, frame);
    }
    return false;
  }

  bool SendBinary(const std::vector<uint8_t> &data) {
    auto frame = CreateWsFrame(0x02, data);
    if (auto l = listener_.lock()) {
      return l->Send(session_id_, frame);
    }
    return false;
  }

  bool Ping(const std::string &payload = "") {
    auto frame = CreateWsFrame(0x09, payload);
    if (auto l = listener_.lock()) {
      return l->Send(session_id_, frame);
    }
    return false;
  }

  void Close(uint16_t status_code = 1000, const std::string &reason = "") {
    std::vector<uint8_t> payload;
    payload.push_back(static_cast<uint8_t>((status_code >> 8) & 0xFF));
    payload.push_back(static_cast<uint8_t>(status_code & 0xFF));
    payload.insert(payload.end(), reason.begin(), reason.end());
    auto frame = CreateWsFrame(0x08, payload);
    if (auto l = listener_.lock()) {
      l->Send(session_id_, frame);
    }
  }

private:
  uint64_t session_id_;
  std::weak_ptr<cpptcpnet::TcpListener> listener_;
};

using WsConnectHandler =
    std::function<void(std::shared_ptr<WebSocketConnection>)>;
using WsMessageHandler = std::function<void(
    std::shared_ptr<WebSocketConnection>, const std::string &)>;
using WsBinaryHandler = std::function<void(std::shared_ptr<WebSocketConnection>,
                                           const std::vector<uint8_t> &)>;
using WsCloseHandler =
    std::function<void(std::shared_ptr<WebSocketConnection>)>;

struct WebSocketBehavior {
  WsConnectHandler on_open;
  WsMessageHandler on_message;
  WsBinaryHandler on_binary;
  WsCloseHandler on_close;

  // Custom connection profile for WebSocket sessions
  cpptcpnet::ConnectionProfile connection_profile = []() {
    cpptcpnet::ConnectionProfile p;
    p.no_delay = true;
    p.keepalive_enabled = true;
    p.keepalive_idle_secs = 60;
    p.keepalive_interval_secs = 15;
    p.keepalive_count = 4;
    p.idle_timeout = std::chrono::milliseconds(3600000); // 1 hour idle timeout
    return p;
  }();
};

/**
 * @brief Represents an incoming HTTP/1.1 request.
 */
struct HttpRequest {
  std::string method;
  std::string path;
  std::string version;
  HeaderMap headers;
  MultiHeaderMap multi_headers;
  std::string body;
  std::unordered_map<std::string, std::string> query_params;
  std::unordered_map<std::string, std::string> path_params;
  std::string client_ip;

  std::string GetHeader(const std::string &key) const {
    auto it = headers.find(key);
    if (it != headers.end()) {
      return it->second;
    }
    return "";
  }
};

/**
 * @brief Represents an outgoing HTTP/1.1 response.
 */
struct HttpResponse {
  int status_code = 200;
  std::string status_message = "OK";
  HeaderMap headers;
  MultiHeaderMap multi_headers;
  std::string body;

  bool is_file = false;
  std::string file_path;

  std::string GetHeader(const std::string &key) const {
    auto it = headers.find(key);
    if (it != headers.end()) {
      return it->second;
    }
    return "";
  }

  static std::string GetDefaultStatusMessage(int code) {
    switch (code) {
    case 100:
      return "Continue";
    case 101:
      return "Switching Protocols";
    case 200:
      return "OK";
    case 201:
      return "Created";
    case 202:
      return "Accepted";
    case 204:
      return "No Content";
    case 301:
      return "Moved Permanently";
    case 302:
      return "Found";
    case 304:
      return "Not Modified";
    case 400:
      return "Bad Request";
    case 401:
      return "Unauthorized";
    case 403:
      return "Forbidden";
    case 404:
      return "Not Found";
    case 405:
      return "Method Not Allowed";
    case 413:
      return "Payload Too Large";
    case 431:
      return "Request Header Fields Too Large";
    case 500:
      return "Internal Server Error";
    case 501:
      return "Not Implemented";
    case 502:
      return "Bad Gateway";
    case 503:
      return "Service Unavailable";
    default:
      return "Unknown";
    }
  }

  static HttpResponse Json(const std::string &json_body,
                           int status_code = 200) {
    HttpResponse res;
    res.status_code = status_code;
    res.status_message = GetDefaultStatusMessage(status_code);
    res.headers["Content-Type"] = "application/json";
    res.body = json_body;
    return res;
  }

  static HttpResponse Html(const std::string &html_body,
                           int status_code = 200) {
    HttpResponse res;
    res.status_code = status_code;
    res.status_message = GetDefaultStatusMessage(status_code);
    res.headers["Content-Type"] = "text/html";
    res.body = html_body;
    return res;
  }

  static HttpResponse Plain(const std::string &text_body,
                            int status_code = 200) {
    HttpResponse res;
    res.status_code = status_code;
    res.status_message = GetDefaultStatusMessage(status_code);
    res.headers["Content-Type"] = "text/plain";
    res.body = text_body;
    return res;
  }

  static HttpResponse Redirect(const std::string &location,
                               int status_code = 302) {
    HttpResponse res;
    res.status_code = status_code;
    res.status_message = GetDefaultStatusMessage(status_code);
    res.headers["Location"] = location;
    return res;
  }

  /**
   * @brief Serializes the response object into a valid HTTP/1.1 raw string.
   */
  std::string Serialize(const std::string &request_method = "",
                        bool headers_only = false) const {
    std::ostringstream oss;
    std::string msg = status_message;
    if (msg == "OK" && status_code != 200) {
      msg = GetDefaultStatusMessage(status_code);
    }

    // Validate request/response splitting
    if (!is_valid_header_value(msg)) {
      throw std::invalid_argument("Invalid status message (contains CRLF)");
    }

    oss << "HTTP/1.1 " << status_code << " " << msg << "\r\n";

    std::unordered_set<std::string, CaseInsensitiveHash, CaseInsensitiveEqual>
        serialized_keys;
    if (!multi_headers.empty()) {
      for (const auto &[key, value] : multi_headers) {
        if (!is_valid_header_value(key) || !is_valid_header_value(value)) {
          throw std::invalid_argument(
              "Invalid header key or value (contains CRLF)");
        }
        oss << key << ": " << value << "\r\n";
        serialized_keys.insert(key);
      }
    }

    for (const auto &[key, value] : headers) {
      if (serialized_keys.find(key) == serialized_keys.end()) {
        if (!is_valid_header_value(key) || !is_valid_header_value(value)) {
          throw std::invalid_argument(
              "Invalid header key or value (contains CRLF)");
        }
        oss << key << ": " << value << "\r\n";
      }
    }

    bool has_te_chunked = false;
    auto te_it = headers.find("transfer-encoding");
    if (te_it != headers.end()) {
      std::string val = te_it->second;
      std::transform(val.begin(), val.end(), val.begin(), safe_tolower);
      if (val.find("chunked") != std::string::npos) {
        has_te_chunked = true;
      }
    }
    if (!has_te_chunked) {
      auto range = multi_headers.equal_range("transfer-encoding");
      for (auto it = range.first; it != range.second; ++it) {
        std::string val = it->second;
        std::transform(val.begin(), val.end(), val.begin(), safe_tolower);
        if (val.find("chunked") != std::string::npos) {
          has_te_chunked = true;
          break;
        }
      }
    }

    bool has_cl =
        (headers.find("content-length") != headers.end()) ||
        (multi_headers.find("content-length") != multi_headers.end()) ||
        has_te_chunked;

    bool allows_body =
        (status_code >= 200 && status_code != 204 && status_code != 304);

    bool is_head = CaseInsensitiveEqual()(request_method, "HEAD");

    if (allows_body && !has_cl) {
      oss << "Content-Length: " << body.length() << "\r\n";
    }

    oss << "\r\n";
    if (headers_only) {
      return oss.str();
    }
    if (allows_body && !is_head) {
      oss << body;
    }
    return oss.str();
  }
};

inline std::string GetMimeType(const std::string &filepath) {
  size_t dot_pos = filepath.find_last_of('.');
  if (dot_pos == std::string::npos)
    return "application/octet-stream";
  std::string ext = filepath.substr(dot_pos + 1);
  std::transform(ext.begin(), ext.end(), ext.begin(), safe_tolower);
  if (ext == "html" || ext == "htm")
    return "text/html";
  if (ext == "css")
    return "text/css";
  if (ext == "js")
    return "application/javascript";
  if (ext == "json")
    return "application/json";
  if (ext == "png")
    return "image/png";
  if (ext == "jpg" || ext == "jpeg")
    return "image/jpeg";
  if (ext == "gif")
    return "image/gif";
  if (ext == "svg")
    return "image/svg+xml";
  if (ext == "txt")
    return "text/plain";
  if (ext == "ico")
    return "image/x-icon";
  if (ext == "xml")
    return "application/xml";
  if (ext == "pdf")
    return "application/pdf";
  return "application/octet-stream";
}

/**
 * @brief Configuration options for CORS middleware.
 */
struct CorsConfig {
  std::string allow_origin = "*";
  std::vector<std::string> allow_methods = {"GET",     "POST",  "PUT", "DELETE",
                                            "OPTIONS", "PATCH", "HEAD"};
  std::vector<std::string> allow_headers = {"*"};
  std::vector<std::string> expose_headers = {};
  bool allow_credentials = false;
  std::chrono::seconds max_age = std::chrono::seconds(86400); // 24 hours
};

/**
 * @brief CORS middleware for handling Cross-Origin Resource Sharing.
 */
class Cors {
public:
  Cors(CorsConfig config = CorsConfig()) : config_(std::move(config)) {}

  bool operator()(HttpRequest &req, HttpResponse &res) {
    // Set Access-Control-Allow-Origin
    res.headers["Access-Control-Allow-Origin"] = config_.allow_origin;

    // Set Access-Control-Allow-Credentials if enabled
    if (config_.allow_credentials) {
      res.headers["Access-Control-Allow-Credentials"] = "true";
    }

    // Set Access-Control-Expose-Headers if any
    if (!config_.expose_headers.empty()) {
      res.headers["Access-Control-Expose-Headers"] =
          join(config_.expose_headers, ", ");
    }

    // Handle preflight requests
    if (req.method == "OPTIONS") {
      if (!config_.allow_methods.empty()) {
        res.headers["Access-Control-Allow-Methods"] =
            join(config_.allow_methods, ", ");
      }
      if (!config_.allow_headers.empty()) {
        res.headers["Access-Control-Allow-Headers"] =
            join(config_.allow_headers, ", ");
      }
      if (config_.max_age.count() > 0) {
        res.headers["Access-Control-Max-Age"] =
            std::to_string(config_.max_age.count());
      }

      // Stop further processing for preflight requests and return 204 No
      // Content
      res.status_code = 204;
      res.status_message = "No Content";
      return false;
    }

    // Continue processing for actual requests
    return true;
  }

private:
  CorsConfig config_;

  static std::string join(const std::vector<std::string> &vec,
                          const std::string &delim) {
    if (vec.empty())
      return "";
    std::string result = vec[0];
    for (size_t i = 1; i < vec.size(); ++i) {
      result += delim + vec[i];
    }
    return result;
  }
};

/**
 * @brief RateLimiter middleware to limit requests per IP address.
 */
class RateLimiter {
public:
  RateLimiter(size_t max_requests, std::chrono::seconds window_duration)
      : max_requests_(max_requests), window_duration_(window_duration),
        state_(std::make_shared<State>()) {}

  bool operator()(HttpRequest &req, HttpResponse &res) {
    std::lock_guard<std::mutex> lock(state_->mutex);
    auto now = std::chrono::steady_clock::now();

    // Clean up expired entries from the front of the queue
    while (!state_->cleanup_queue.empty()) {
      const auto &front = state_->cleanup_queue.front();
      if (now - front.window_start >= window_duration_) {
        auto it = state_->clients.find(front.client_ip);
        if (it != state_->clients.end()) {
          if (it->second.window_start == front.window_start) {
            state_->clients.erase(it);
          }
        }
        state_->cleanup_queue.pop_front();
      } else {
        break;
      }
    }

    auto &client = state_->clients[req.client_ip];
    if (client.count == 0 || now - client.window_start >= window_duration_) {
      client.count = 1;
      client.window_start = now;
      state_->cleanup_queue.push_back({req.client_ip, now});
      return true;
    }

    if (client.count < max_requests_) {
      client.count++;
      return true;
    }

    res.status_code = 429;
    res.status_message = "Too Many Requests";
    res.headers["Content-Type"] = "text/plain";
    res.headers["Retry-After"] = std::to_string(window_duration_.count());
    res.body = "Rate limit exceeded. Please try again later.";
    return false; // Stop further processing
  }

private:
  struct ClientState {
    size_t count = 0;
    std::chrono::steady_clock::time_point window_start;
  };

  struct QueueEntry {
    std::string client_ip;
    std::chrono::steady_clock::time_point window_start;
  };

  struct State {
    std::mutex mutex;
    std::unordered_map<std::string, ClientState> clients;
    std::deque<QueueEntry> cleanup_queue;
  };

  size_t max_requests_;
  std::chrono::seconds window_duration_;
  std::shared_ptr<State> state_;
};

/**
 * @brief ResponseCache wraps a RouteHandler to cache its responses.
 */
class ResponseCache {
private:
  struct CacheEntry {
    HttpResponse response;
    std::chrono::steady_clock::time_point expires_at;
  };

  struct CacheQueueEntry {
    std::string key;
    std::chrono::steady_clock::time_point expires_at;
  };

  struct CacheState {
    std::mutex mutex;
    std::unordered_map<std::string, CacheEntry> cache;
    std::deque<CacheQueueEntry> cleanup_queue;
  };

public:
  ResponseCache(std::chrono::seconds default_duration)
      : default_duration_(default_duration),
        state_(std::make_shared<CacheState>()) {}

  std::function<HttpResponse(const HttpRequest &)>
  Wrap(std::function<HttpResponse(const HttpRequest &)> handler,
       std::optional<std::chrono::seconds> duration = std::nullopt) {

    std::chrono::seconds cache_dur = duration.value_or(default_duration_);
    auto state = state_;

    return [state, handler, cache_dur](const HttpRequest &req) {
      // Only cache GET requests
      if (req.method != "GET") {
        return handler(req);
      }

      // We use path + query string as cache key
      std::string cache_key = req.path;
      if (!req.query_params.empty()) {
        cache_key += "?";
        bool first = true;
        for (const auto &kv : req.query_params) {
          if (!first) {
            cache_key += "&";
          }
          cache_key += kv.first + "=" + kv.second;
          first = false;
        }
      }

      auto now = std::chrono::steady_clock::now();

      {
        std::lock_guard<std::mutex> lock(state->mutex);

        // Clean up expired entries from the front of the queue
        while (!state->cleanup_queue.empty()) {
          const auto &front = state->cleanup_queue.front();
          if (now >= front.expires_at) {
            auto it = state->cache.find(front.key);
            if (it != state->cache.end()) {
              if (it->second.expires_at == front.expires_at) {
                state->cache.erase(it);
              }
            }
            state->cleanup_queue.pop_front();
          } else {
            break;
          }
        }

        auto it = state->cache.find(cache_key);
        if (it != state->cache.end()) {
          if (now < it->second.expires_at) {
            return it->second.response;
          } else {
            state->cache.erase(it);
          }
        }
      }

      // Cache miss - call handler
      HttpResponse res = handler(req);

      // Only cache 200 OK responses, and skip files as caching large files in
      // memory is bad
      if (res.status_code == 200 && !res.is_file) {
        std::lock_guard<std::mutex> lock(state->mutex);
        auto expires_at = std::chrono::steady_clock::now() + cache_dur;
        state->cache[cache_key] = {res, expires_at};
        state->cleanup_queue.push_back({cache_key, expires_at});
      }

      return res;
    };
  }

private:
  std::chrono::seconds default_duration_;
  std::shared_ptr<CacheState> state_;
};

/**
 * @brief A high-performance, asynchronous HTTP server built on cpptcpnet.
 */
class HttpServer {
public:
  using RouteHandler = std::function<HttpResponse(const HttpRequest &)>;
  using Middleware = std::function<bool(HttpRequest &, HttpResponse &)>;
  using StreamHandler = std::function<std::optional<HttpResponse>(
      const HttpRequest &, const std::string &, bool)>;

  HttpServer(const HttpServer &) = delete;
  HttpServer &operator=(const HttpServer &) = delete;
  HttpServer(HttpServer &&) = delete;
  HttpServer &operator=(HttpServer &&) = delete;

  struct SessionState {
    std::mutex mutex;
    std::deque<uint8_t> buffer;
    bool is_websocket = false;
    std::shared_ptr<WebSocketConnection> ws_connection;
    WebSocketBehavior ws_behavior;

    bool ws_fragmented = false;
    uint8_t ws_fragmented_opcode = 0;
    std::vector<uint8_t> ws_assembled_payload;

    std::string partial_body;
    size_t chunk_parsed_up_to = 0;
    bool parsing_chunked = false;

    // Stream upload and download fields
    bool is_streaming_file = false;
    std::shared_ptr<std::ifstream> file_stream;
    size_t file_bytes_in_flight = 0;
    bool should_close_after_file = false;

    bool header_parsed = false;
    HttpRequest request;
    size_t body_start_idx = 0;
    bool is_stream_route = false;
    StreamHandler stream_handler;
    size_t content_length = 0;
    bool is_chunked = false;
    size_t stream_bytes_received = 0;
  };

  /**
   * @brief Constructs a new HttpServer.
   * @param port The port to listen on.
   * @param bind_address The IP address to bind the listener to (defaults to
   * "0.0.0.0").
   */
  HttpServer(uint16_t port, const std::string &bind_address = "0.0.0.0")
      : listener_(
            std::make_shared<cpptcpnet::TcpListener>(port, bind_address)) {
    listener_->SetDataHandler(
        [this](uint64_t session_id, const std::vector<uint8_t> &data) {
          this->HandleIncomingData(session_id, data);
        });

    state_sub_ =
        listener_->GetEventBroker().Subscribe<cpptcpnet::ConnectionEvent>(
            "state_events");

    transfer_sub_ =
        listener_->GetEventBroker().Subscribe<cpptcpnet::TransferEvent>(
            "transfer_events");
  }

  ~HttpServer() {
    Stop();
    if (background_thread_.joinable()) {
      background_thread_.join();
    }
  }

  void Use(Middleware middleware) {
    middlewares_.push_back(std::move(middleware));
  }

  void Use(const std::string &path_prefix, Middleware middleware) {
    middlewares_.push_back([path_prefix, mw = std::move(middleware)](
                               HttpRequest &req, HttpResponse &res) {
      if (req.path.rfind(path_prefix, 0) == 0) {
        return mw(req, res);
      }
      return true;
    });
  }

  void Get(const std::string &path, RouteHandler handler) {
    AddRoute("GET", path, std::move(handler));
  }

  void Post(const std::string &path, RouteHandler handler) {
    AddRoute("POST", path, std::move(handler));
  }

  void PostStream(const std::string &path, StreamHandler handler) {
    AddStreamRoute("POST", path, std::move(handler));
  }

  void Put(const std::string &path, RouteHandler handler) {
    AddRoute("PUT", path, std::move(handler));
  }

  void PutStream(const std::string &path, StreamHandler handler) {
    AddStreamRoute("PUT", path, std::move(handler));
  }

  void Delete(const std::string &path, RouteHandler handler) {
    AddRoute("DELETE", path, std::move(handler));
  }

  void Patch(const std::string &path, RouteHandler handler) {
    AddRoute("PATCH", path, std::move(handler));
  }

  void Options(const std::string &path, RouteHandler handler) {
    AddRoute("OPTIONS", path, std::move(handler));
  }

  void Head(const std::string &path, RouteHandler handler) {
    AddRoute("HEAD", path, std::move(handler));
  }

  void StaticDir(const std::string &route_prefix,
                 const std::string &directory_path, bool spa_mode = false) {
    std::string route = route_prefix;
    if (!route.empty() && route.back() != '/') {
      route += "/";
    }
    route += "*";

    std::string dir = directory_path;
    if (!dir.empty() && dir.back() == '/') {
      dir.pop_back();
    }

    Get(route, [dir, spa_mode](const HttpRequest &req) {
      std::string filepath = dir + "/" + req.path_params.at("*");

      std::error_code ec;
      std::string canonical_dir =
          std::filesystem::weakly_canonical(dir, ec).string();
      std::string canonical_filepath =
          std::filesystem::weakly_canonical(filepath, ec).string();

      auto handle_fallback = [&]() -> HttpResponse {
        if (spa_mode) {
          std::string spa_root_index = dir + "/index.html";
          std::string canonical_spa_root =
              std::filesystem::weakly_canonical(spa_root_index, ec).string();
          std::ifstream file(canonical_spa_root, std::ios::binary);
          if (file.is_open()) {
            HttpResponse res;
            res.status_code = 200;
            res.status_message = "OK";
            res.is_file = true;
            res.file_path = canonical_spa_root;
            res.headers["Content-Type"] = "text/html";
            res.headers["Content-Length"] = std::to_string(
                std::filesystem::file_size(canonical_spa_root, ec));
            return res;
          }
        }
        return HttpResponse::Plain("Not Found", 404);
      };

      // Path traversal check
      if (canonical_filepath.rfind(canonical_dir, 0) != 0 ||
          (canonical_filepath.size() > canonical_dir.size() &&
           canonical_dir.back() != '/' &&
           canonical_filepath[canonical_dir.size()] != '/')) {
        return handle_fallback();
      }

      if (std::filesystem::is_directory(canonical_filepath, ec)) {
        // Look for index.html in the directory
        std::string index_path = canonical_filepath + "/index.html";
        std::string canonical_index =
            std::filesystem::weakly_canonical(index_path, ec).string();
        std::ifstream file(canonical_index, std::ios::binary);
        if (file.is_open()) {
          canonical_filepath = canonical_index;
        } else {
          return handle_fallback();
        }
      }

      // Check if file exists and can be opened
      {
        std::ifstream file(canonical_filepath, std::ios::binary);
        if (!file.is_open()) {
          return handle_fallback();
        }
      }

      HttpResponse res;
      res.status_code = 200;
      res.status_message = "OK";
      res.is_file = true;
      res.file_path = canonical_filepath;
      res.headers["Content-Type"] = GetMimeType(canonical_filepath);
      res.headers["Content-Length"] =
          std::to_string(std::filesystem::file_size(canonical_filepath, ec));
      return res;
    });
  }

  void WebSocket(const std::string &path, WebSocketBehavior behavior) {
    ws_routes_[path] = behavior;
    ws_routes_list_.push_back({path, split_path(path), behavior});
  }

  void Start() {
    {
      std::lock_guard<std::mutex> lock(shutdown_mutex_);
      if (running_) {
        return;
      }
      running_ = true;
      background_thread_ = std::thread([this]() {
        while (running_) {
          bool idle = true;
          if (auto event = transfer_sub_->try_receive()) {
            idle = false;
            if (event->is_send) {
              {
                std::lock_guard<std::mutex> lock(close_mutex_);
                auto it = sessions_to_close_.find(event->session_id);
                if (it != sessions_to_close_.end()) {
                  auto &pending = pending_send_bytes_[event->session_id];
                  if (event->bytes_transferred >= pending) {
                    pending = 0;
                  } else {
                    pending -= event->bytes_transferred;
                  }
                  if (pending == 0) {
                    listener_->Disconnect(event->session_id);
                    sessions_to_close_.erase(it);
                    pending_send_bytes_.erase(event->session_id);
                  }
                }
              }
              std::shared_ptr<SessionState> session;
              {
                std::lock_guard<std::mutex> lock(session_mutex_);
                auto it = sessions_.find(event->session_id);
                if (it != sessions_.end()) {
                  session = it->second;
                }
              }
              if (session) {
                std::lock_guard<std::mutex> session_lock(session->mutex);
                if (session->is_streaming_file) {
                  if (session->file_bytes_in_flight >=
                      event->bytes_transferred) {
                    session->file_bytes_in_flight -= event->bytes_transferred;
                  } else {
                    session->file_bytes_in_flight = 0;
                  }
                  PumpFileTransfer(event->session_id, session);
                }
              }
            }
          }

          if (auto event = state_sub_->try_receive()) {
            idle = false;
            if (event->state == cpptcpnet::ConnectionState::Disconnected) {
              std::shared_ptr<SessionState> session_to_close;
              {
                std::lock_guard<std::mutex> lock(session_mutex_);
                auto it = sessions_.find(event->session_id);
                if (it != sessions_.end()) {
                  session_to_close = it->second;
                  sessions_.erase(it);
                }
              }
              if (session_to_close) {
                std::lock_guard<std::mutex> session_lock(
                    session_to_close->mutex);
                if (session_to_close->is_websocket &&
                    session_to_close->ws_behavior.on_close) {
                  session_to_close->ws_behavior.on_close(
                      session_to_close->ws_connection);
                }
              }
              {
                std::lock_guard<std::mutex> lock(close_mutex_);
                sessions_to_close_.erase(event->session_id);
                pending_send_bytes_.erase(event->session_id);
              }
            }
          }

          if (idle) {
            std::unique_lock<std::mutex> lock(shutdown_mutex_);
            shutdown_cv_.wait_for(lock, idle_timeout_,
                                  [this]() { return !running_; });
          }
        }
      });
    }
    listener_->Start();
  }

  void Stop() {
    std::thread thread_to_join;
    {
      std::lock_guard<std::mutex> lock(shutdown_mutex_);
      if (!running_) {
        listener_->Stop();
        return;
      }
      running_ = false;
      shutdown_cv_.notify_all();
      thread_to_join = std::move(background_thread_);
    }
    if (thread_to_join.joinable()) {
      thread_to_join.join();
    }
    listener_->Stop();
  }

  cpptcpnet::TcpListener &GetListener() { return *listener_; }
  const cpptcpnet::TcpListener &GetListener() const { return *listener_; }

  void SetMaxHeaderSize(size_t size) { max_header_size_ = size; }
  size_t GetMaxHeaderSize() const { return max_header_size_; }

  void SetMaxBodySize(size_t size) { max_body_size_ = size; }
  size_t GetMaxBodySize() const { return max_body_size_; }

  void SetIdleTimeout(std::chrono::milliseconds timeout) {
    idle_timeout_ = timeout;
  }
  std::chrono::milliseconds GetIdleTimeout() const { return idle_timeout_; }

private:
  struct Route {
    std::string method;
    std::string path_pattern;
    std::vector<std::string> segments;
    RouteHandler handler;
  };

  struct WsRoute {
    std::string path_pattern;
    std::vector<std::string> segments;
    WebSocketBehavior behavior;
  };

  std::shared_ptr<cpptcpnet::TcpListener> listener_;
  std::thread background_thread_;
  // routes_ provides fast O(1) exact-match lookups for requests without
  // parameters.
  std::unordered_map<std::string, RouteHandler> routes_;
  // routes_list_ stores all routes in a vector to support O(N) path-pattern
  // matching (e.g. dynamic parameters).
  std::vector<Route> routes_list_;
  std::vector<Middleware> middlewares_;
  std::atomic<bool> running_{false};
  std::mutex shutdown_mutex_;
  std::condition_variable shutdown_cv_;

  std::shared_ptr<cpppubsub::Subscriber<cpptcpnet::ConnectionEvent>> state_sub_;
  std::shared_ptr<cpppubsub::Subscriber<cpptcpnet::TransferEvent>>
      transfer_sub_;

  std::unordered_map<uint64_t, std::shared_ptr<SessionState>> sessions_;
  std::mutex session_mutex_;

  std::unordered_map<std::string, WebSocketBehavior> ws_routes_;
  std::vector<WsRoute> ws_routes_list_;

  struct StreamRoute {
    std::string method;
    std::string path_pattern;
    std::vector<std::string> segments;
    StreamHandler handler;
  };

  std::unordered_map<std::string, StreamHandler> stream_routes_;
  std::vector<StreamRoute> stream_routes_list_;

  std::unordered_set<uint64_t> sessions_to_close_;
  std::unordered_map<uint64_t, size_t> pending_send_bytes_;
  std::mutex close_mutex_;
  size_t max_header_size_ = 8192;
  size_t max_body_size_ = 10 * 1024 * 1024;
  std::chrono::milliseconds idle_timeout_{10};

  void PumpFileTransfer(uint64_t session_id,
                        std::shared_ptr<SessionState> session) {
    if (!session->is_streaming_file || !session->file_stream ||
        !session->file_stream->is_open())
      return;

    size_t max_flight =
        listener_->GetConnectionProfile(session_id).max_outbound_buffer_size /
        2;
    if (max_flight < 65536)
      max_flight = 65536;

    size_t chunk_size =
        listener_->GetConnectionProfile(session_id).send_chunk_size;
    if (chunk_size == 0)
      chunk_size = 65536;

    while (session->file_bytes_in_flight < max_flight &&
           *session->file_stream && running_) {
      std::vector<char> stream_buffer(chunk_size);
      session->file_stream->read(stream_buffer.data(), stream_buffer.size());
      std::streamsize bytes_read = session->file_stream->gcount();
      if (bytes_read > 0) {
        std::string chunk(stream_buffer.data(), bytes_read);
        if (listener_->Send(session_id, chunk)) {
          session->file_bytes_in_flight += bytes_read;
        } else {
          break;
        }
      } else {
        break;
      }
    }

    if (!*session->file_stream || !session->file_stream->is_open() ||
        session->file_stream->eof()) {
      if (session->file_bytes_in_flight == 0) {
        session->is_streaming_file = false;
        session->file_stream.reset();
        if (session->should_close_after_file) {
          listener_->Disconnect(session_id);
        }
      }
    }
  }

  bool HandleStreamRoute(uint64_t session_id,
                         std::shared_ptr<SessionState> session) {
    auto &buffer = session->buffer;
    std::optional<HttpResponse> handler_response;
    bool completed = false;

    if (session->is_chunked) {
      bool chunked_bad_request = false;
      bool chunked_payload_too_large = false;
      completed = ParseChunkedBodyIncremental(
          buffer, session->chunk_parsed_up_to, session->partial_body,
          chunked_bad_request, max_body_size_, chunked_payload_too_large,
          [&](const std::string &chunk) {
            if (auto res =
                    session->stream_handler(session->request, chunk, false)) {
              handler_response = res;
            }
          });

      if (chunked_payload_too_large) {
        HttpResponse response =
            HttpResponse::Plain("413 Payload Too Large", 413);
        response.headers["Connection"] = "close";
        std::string response_data = response.Serialize();
        {
          std::lock_guard<std::mutex> close_lock(close_mutex_);
          sessions_to_close_.insert(session_id);
          pending_send_bytes_[session_id] = response_data.size();
        }
        listener_->Send(session_id, response_data);
        return false;
      }

      if (chunked_bad_request) {
        HttpResponse response = HttpResponse::Plain("400 Bad Request", 400);
        response.headers["Connection"] = "close";
        std::string response_data = response.Serialize();
        {
          std::lock_guard<std::mutex> close_lock(close_mutex_);
          sessions_to_close_.insert(session_id);
          pending_send_bytes_[session_id] = response_data.size();
        }
        listener_->Send(session_id, response_data);
        return false;
      }

      if (!handler_response && !completed) {
        buffer.erase(buffer.begin() + session->body_start_idx,
                     buffer.begin() + session->chunk_parsed_up_to);
        session->chunk_parsed_up_to = session->body_start_idx;
        return false;
      }
    } else {
      size_t available = buffer.size() - session->body_start_idx;
      size_t to_process = std::min(
          available, session->content_length - session->stream_bytes_received);
      if (to_process > 0) {
        std::string chunk(buffer.begin() + session->body_start_idx,
                          buffer.begin() + session->body_start_idx +
                              to_process);
        if (auto res =
                session->stream_handler(session->request, chunk, false)) {
          handler_response = res;
        }
        session->stream_bytes_received += to_process;
        buffer.erase(buffer.begin() + session->body_start_idx,
                     buffer.begin() + session->body_start_idx + to_process);
      }
      completed = (session->stream_bytes_received >= session->content_length);
    }

    if (handler_response) {
      completed = true;
    } else if (completed) {
      handler_response = session->stream_handler(session->request, "", true);
    }

    if (completed) {
      if (handler_response) {
        std::string response_data;
        std::string headers_data;
        if (handler_response->is_file) {
          headers_data =
              handler_response->Serialize(session->request.method, true);
          listener_->Send(session_id, headers_data);
          if (session->request.method != "HEAD") {
            session->is_streaming_file = true;
            session->file_stream = std::make_shared<std::ifstream>(
                handler_response->file_path, std::ios::binary);
            session->file_bytes_in_flight = headers_data.size();

            std::string conn_header = session->request.GetHeader("Connection");
            std::transform(conn_header.begin(), conn_header.end(),
                           conn_header.begin(), safe_tolower);
            bool should_close =
                (conn_header.find("close") != std::string::npos) ||
                (session->request.version == "HTTP/1.0" &&
                 conn_header.find("keep-alive") == std::string::npos);
            session->should_close_after_file = should_close;
            PumpFileTransfer(session_id, session);
          }
        } else {
          response_data = handler_response->Serialize(session->request.method);
          listener_->Send(session_id, response_data);
        }
      }
      buffer.erase(buffer.begin(), buffer.begin() + session->body_start_idx);
      session->header_parsed = false;
      session->partial_body.clear();
      session->chunk_parsed_up_to = 0;
      session->parsing_chunked = false;
      return true;
    }

    return false;
  }

  void AddRoute(const std::string &method, const std::string &path,
                RouteHandler handler) {
    routes_[method + ":" + path] = handler;
    routes_list_.push_back({method, path, split_path(path), handler});
  }

  void AddStreamRoute(const std::string &method, const std::string &path,
                      StreamHandler handler) {
    stream_routes_[method + ":" + path] = handler;
    stream_routes_list_.push_back({method, path, split_path(path), handler});
  }

  void HandleIncomingData(uint64_t session_id,
                          const std::vector<uint8_t> &data) {
    std::shared_ptr<SessionState> session;
    {
      std::lock_guard<std::mutex> lock(session_mutex_);
      auto it = sessions_.find(session_id);
      if (it == sessions_.end()) {
        session = std::make_shared<SessionState>();
        sessions_[session_id] = session;
      } else {
        session = it->second;
      }
    }

    std::lock_guard<std::mutex> session_lock(session->mutex);

    auto &buffer = session->buffer;

    // Optimization: start searching for '\r\n\r\n' only from the end of the
    // previous buffer minus 3 bytes. This is safe because if a CRLFCRLF
    // terminator spans the boundary between the old buffer and the newly
    // appended data, it must start at least at buffer.size() - 3. If it started
    // earlier, it would have been fully contained in the old buffer, which
    // already failed to find a terminator.
    size_t search_start = 0;
    const std::string terminator = "\r\n\r\n";
    auto it = std::search(buffer.begin(), buffer.end(), terminator.begin(),
                          terminator.end());
    if (it == buffer.end()) {
      if (buffer.size() + data.size() > max_header_size_) {
        HttpResponse response;
        response.status_code = 431;
        response.status_message = "Request Header Fields Too Large";
        response.headers["Content-Type"] = "text/plain";
        response.headers["Connection"] = "close";
        response.body = "431 Request Header Fields Too Large";
        std::string response_data = response.Serialize();
        {
          std::lock_guard<std::mutex> close_lock(close_mutex_);
          sessions_to_close_.insert(session_id);
          pending_send_bytes_[session_id] = response_data.size();
        }
        listener_->Send(session_id, response_data);
        return;
      }
      if (buffer.size() > 3) {
        search_start = buffer.size() - 3;
      }
    }

    buffer.insert(buffer.end(), data.begin(), data.end());

    while (true) {
      if (session->is_websocket) {
        auto safe_close_and_disconnect = [&](uint16_t status_code,
                                             const std::string &reason) {
          std::vector<uint8_t> payload;
          payload.push_back(static_cast<uint8_t>((status_code >> 8) & 0xFF));
          payload.push_back(static_cast<uint8_t>(status_code & 0xFF));
          payload.insert(payload.end(), reason.begin(), reason.end());
          size_t close_frame_size = CreateWsFrame(0x08, payload).size();
          {
            std::lock_guard<std::mutex> close_lock(close_mutex_);
            sessions_to_close_.insert(session_id);
            pending_send_bytes_[session_id] = close_frame_size;
          }
          session->ws_connection->Close(status_code, reason);
        };

        WsFrame frame;
        bool protocol_error = false;
        bool payload_too_big = false;
        bool parsed = ParseWsFrame(buffer, frame, protocol_error,
                                   payload_too_big, true, max_body_size_);
        if (protocol_error) {
          safe_close_and_disconnect(1002, "Protocol error");
          return;
        }
        if (payload_too_big) {
          safe_close_and_disconnect(1009, "Message too big");
          return;
        }
        if (parsed) {
          if (frame.opcode == 0x08) {
            safe_close_and_disconnect(1000, "Goodbye");
            return;
          } else if (frame.opcode == 0x09) {
            auto pong_frame = CreateWsFrame(0x0A, frame.payload);
            listener_->Send(session_id, pong_frame);
          } else if (frame.opcode == 0x0A) {
            // Handle Pong
          } else if (frame.opcode == 0x01 || frame.opcode == 0x02 ||
                     frame.opcode == 0x00) {
            if (frame.opcode != 0x00) {
              if (session->ws_fragmented) {
                safe_close_and_disconnect(1002, "Protocol error");
                return;
              } else {
                session->ws_fragmented_opcode = frame.opcode;
                session->ws_fragmented = !frame.fin;
                if (frame.payload.size() > max_body_size_) {
                  safe_close_and_disconnect(1009, "Message too big");
                  return;
                }
                session->ws_assembled_payload = std::move(frame.payload);
              }
            } else {
              if (!session->ws_fragmented) {
                safe_close_and_disconnect(1002, "Protocol error");
                return;
              } else {
                if (session->ws_assembled_payload.size() +
                        frame.payload.size() >
                    max_body_size_) {
                  safe_close_and_disconnect(1009, "Message too big");
                  return;
                }
                session->ws_assembled_payload.insert(
                    session->ws_assembled_payload.end(), frame.payload.begin(),
                    frame.payload.end());
                session->ws_fragmented = !frame.fin;
              }
            }

            if (session->ws_assembled_payload.size() > max_body_size_) {
              safe_close_and_disconnect(1009, "Message too big");
              return;
            }

            if (frame.fin) {
              uint8_t final_opcode = session->ws_fragmented_opcode;
              session->ws_fragmented = false;
              session->ws_fragmented_opcode = 0;

              if (final_opcode == 0x01) {
                if (session->ws_behavior.on_message) {
                  std::string text(session->ws_assembled_payload.begin(),
                                   session->ws_assembled_payload.end());
                  session->ws_behavior.on_message(session->ws_connection, text);
                }
              } else if (final_opcode == 0x02) {
                if (session->ws_behavior.on_binary) {
                  session->ws_behavior.on_binary(session->ws_connection,
                                                 session->ws_assembled_payload);
                }
              }
              session->ws_assembled_payload.clear();
            }
          }
          continue;
        } else {
          break;
        }
      }

      if (session->header_parsed) {
        if (session->is_stream_route) {
          if (HandleStreamRoute(session_id, session)) {
            continue;
          } else {
            break;
          }
        } else {
          session->header_parsed = false;
        }
      }

      auto it_term = std::search(buffer.begin() + search_start, buffer.end(),
                                 terminator.begin(), terminator.end());
      if (it_term == buffer.end()) {
        break;
      }

      size_t header_end_idx = std::distance(buffer.begin(), it_term);
      size_t body_start_idx = header_end_idx + 4;

      std::string raw_header(buffer.begin(), buffer.begin() + header_end_idx);

      try {
        HttpRequest temp_request = ParseHeader(raw_header);

        std::vector<std::string> request_segments =
            split_path(temp_request.path);
        std::string method_path = temp_request.method + ":" + temp_request.path;
        auto stream_it = stream_routes_.find(method_path);
        bool is_stream = false;
        StreamHandler handler;
        if (stream_it != stream_routes_.end()) {
          is_stream = true;
          handler = stream_it->second;
        } else {
          for (const auto &route : stream_routes_list_) {
            if (match_route(temp_request.method, request_segments, route.method,
                            route.segments, temp_request.path_params)) {
              is_stream = true;
              handler = route.handler;
              break;
            }
          }
        }

        if (is_stream) {
          session->request = std::move(temp_request);
          std::string forwarded = session->request.GetHeader("X-Forwarded-For");
          if (!forwarded.empty()) {
            size_t comma_pos = forwarded.find(',');
            if (comma_pos != std::string::npos) {
              session->request.client_ip = forwarded.substr(0, comma_pos);
            } else {
              session->request.client_ip = forwarded;
            }
          } else {
            auto peer_addr = listener_->GetPeerAddress(session_id);
            session->request.client_ip = peer_addr.ip;
          }

          if (session->request.method.empty() ||
              session->request.path.empty() ||
              session->request.version.empty()) {
            HttpResponse response = HttpResponse::Plain("400 Bad Request", 400);
            response.headers["Connection"] = "close";
            std::string response_data = response.Serialize();
            {
              std::lock_guard<std::mutex> close_lock(close_mutex_);
              sessions_to_close_.insert(session_id);
              pending_send_bytes_[session_id] = response_data.size();
            }
            listener_->Send(session_id, response_data);
            return;
          }

          std::string te_val = session->request.GetHeader("Transfer-Encoding");
          std::transform(te_val.begin(), te_val.end(), te_val.begin(),
                         safe_tolower);
          session->is_chunked = (te_val.find("chunked") != std::string::npos);
          if (!session->is_chunked) {
            std::string cl_val = session->request.GetHeader("Content-Length");
            session->content_length = cl_val.empty() ? 0 : std::stoull(cl_val);
          }
          session->stream_bytes_received = 0;
          session->chunk_parsed_up_to = body_start_idx;
          session->body_start_idx = body_start_idx;
          session->is_stream_route = true;
          session->stream_handler = handler;
          session->header_parsed = true;

          if (HandleStreamRoute(session_id, session)) {
            continue;
          } else {
            break;
          }
        }
      } catch (const std::exception &e) {
        HttpResponse response = HttpResponse::Plain("400 Bad Request", 400);
        response.headers["Connection"] = "close";
        std::string response_data = response.Serialize();
        {
          std::lock_guard<std::mutex> close_lock(close_mutex_);
          sessions_to_close_.insert(session_id);
          pending_send_bytes_[session_id] = response_data.size();
        }
        listener_->Send(session_id, response_data);
        return;
      }

      try {
        HttpRequest request = ParseHeader(raw_header);

        std::string forwarded = request.GetHeader("X-Forwarded-For");
        if (!forwarded.empty()) {
          size_t comma_pos = forwarded.find(',');
          if (comma_pos != std::string::npos) {
            request.client_ip = forwarded.substr(0, comma_pos);
          } else {
            request.client_ip = forwarded;
          }
        } else {
          auto peer_addr = listener_->GetPeerAddress(session_id);
          request.client_ip = peer_addr.ip;
        }

        if (request.method.empty() || request.path.empty() ||
            request.version.empty()) {
          HttpResponse response;
          response.status_code = 400;
          response.status_message = "Bad Request";
          response.headers["Content-Type"] = "text/plain";
          response.headers["Connection"] = "close";
          response.body = "Malformed HTTP request.";
          std::string response_data = response.Serialize();
          {
            std::lock_guard<std::mutex> close_lock(close_mutex_);
            sessions_to_close_.insert(session_id);
            pending_send_bytes_[session_id] = response_data.size();
          }
          listener_->Send(session_id, response_data);
          return;
        }

        size_t content_length = 0;
        size_t consumed_body_bytes = 0;
        bool is_chunked = false;
        bool chunked_bad_request = false;
        bool chunked_payload_too_large = false;

        std::string te_val = request.GetHeader("Transfer-Encoding");
        std::transform(te_val.begin(), te_val.end(), te_val.begin(),
                       safe_tolower);

        if (te_val.find("chunked") != std::string::npos) {
          is_chunked = true;
          if (!session->parsing_chunked) {
            session->parsing_chunked = true;
            session->chunk_parsed_up_to = body_start_idx;
            session->partial_body.clear();
          }

          bool completed = ParseChunkedBodyIncremental(
              buffer, session->chunk_parsed_up_to, session->partial_body,
              chunked_bad_request, max_body_size_, chunked_payload_too_large);

          request.body = session->partial_body;
          consumed_body_bytes = session->chunk_parsed_up_to - body_start_idx;

          if (chunked_payload_too_large) {
            HttpResponse response =
                HttpResponse::Plain("413 Payload Too Large", 413);
            response.headers["Connection"] = "close";
            std::string response_data = response.Serialize();
            {
              std::lock_guard<std::mutex> close_lock(close_mutex_);
              sessions_to_close_.insert(session_id);
              pending_send_bytes_[session_id] = response_data.size();
            }
            listener_->Send(session_id, response_data);
            return;
          }

          if (chunked_bad_request) {
            HttpResponse response = HttpResponse::Plain("400 Bad Request", 400);
            response.headers["Connection"] = "close";
            std::string response_data = response.Serialize();
            {
              std::lock_guard<std::mutex> close_lock(close_mutex_);
              sessions_to_close_.insert(session_id);
              pending_send_bytes_[session_id] = response_data.size();
            }
            listener_->Send(session_id, response_data);
            return;
          }

          if (!completed) {
            return;
          }
        } else {
          std::string cl_val = request.GetHeader("Content-Length");
          if (!cl_val.empty()) {
            try {
              content_length = std::stoull(cl_val);
            } catch (const std::exception &) {
              HttpResponse response =
                  HttpResponse::Plain("400 Bad Request", 400);
              response.headers["Connection"] = "close";
              std::string response_data = response.Serialize();
              {
                std::lock_guard<std::mutex> close_lock(close_mutex_);
                sessions_to_close_.insert(session_id);
                pending_send_bytes_[session_id] = response_data.size();
              }
              listener_->Send(session_id, response_data);
              return;
            }
          }

          if (content_length > max_body_size_) {
            HttpResponse response =
                HttpResponse::Plain("413 Payload Too Large", 413);
            response.headers["Connection"] = "close";
            std::string response_data = response.Serialize();
            {
              std::lock_guard<std::mutex> close_lock(close_mutex_);
              sessions_to_close_.insert(session_id);
              pending_send_bytes_[session_id] = response_data.size();
            }
            listener_->Send(session_id, response_data);
            return;
          }

          if (buffer.size() - body_start_idx < content_length) {
            return;
          }

          if (content_length > 0) {
            request.body =
                std::string(buffer.begin() + body_start_idx,
                            buffer.begin() + body_start_idx + content_length);
          }
          consumed_body_bytes = content_length;
        }

        bool upgrade_to_ws = false;
        WebSocketBehavior ws_behavior;

        bool should_close = false;
        std::string conn_header = request.GetHeader("Connection");
        std::transform(conn_header.begin(), conn_header.end(),
                       conn_header.begin(), safe_tolower);
        if (conn_header.find("close") != std::string::npos) {
          should_close = true;
        } else if (request.version == "HTTP/1.0" &&
                   conn_header.find("keep-alive") == std::string::npos) {
          should_close = true;
        }

        HttpResponse response =
            DispatchRequest(request, session_id, upgrade_to_ws, ws_behavior);

        if (should_close && !upgrade_to_ws) {
          response.headers["Connection"] = "close";
        }

        std::string accept_encoding = request.GetHeader("Accept-Encoding");
        std::transform(accept_encoding.begin(), accept_encoding.end(),
                       accept_encoding.begin(), safe_tolower);
        if (accept_encoding.find("gzip") != std::string::npos &&
            !response.body.empty() &&
            response.headers.count("Content-Encoding") == 0) {
          auto compressed = gzip_compress(response.body);
          if (compressed.has_value()) {
            response.body = std::move(*compressed);
            response.headers["Content-Encoding"] = "gzip";
            response.headers.erase("content-length");
          }
        }

        std::string response_data;
        std::string headers_data;
        if (response.is_file) {
          headers_data = response.Serialize(request.method, true);
        } else {
          response_data = response.Serialize(request.method);
        }

        std::error_code size_ec;
        size_t file_size = 0;
        if (response.is_file) {
          file_size = std::filesystem::file_size(response.file_path, size_ec);
        }
        size_t total_sent = response.is_file
                                ? (headers_data.size() +
                                   (request.method != "HEAD" ? file_size : 0))
                                : response_data.size();

        if (should_close && !upgrade_to_ws) {
          std::lock_guard<std::mutex> lock(close_mutex_);
          sessions_to_close_.insert(session_id);
          pending_send_bytes_[session_id] = total_sent;
        }

        if (response.is_file) {
          listener_->Send(session_id, headers_data);
          if (request.method != "HEAD") {
            session->is_streaming_file = true;
            session->file_stream = std::make_shared<std::ifstream>(
                response.file_path, std::ios::binary);
            session->file_bytes_in_flight = headers_data.size();
            session->should_close_after_file = should_close && !upgrade_to_ws;
            PumpFileTransfer(session_id, session);
          }
        } else {
          listener_->Send(session_id, response_data);
        }

        if (upgrade_to_ws) {
          listener_->ApplyConnectionProfile(session_id,
                                            ws_behavior.connection_profile);
          session->is_websocket = true;
          session->ws_connection =
              std::make_shared<WebSocketConnection>(session_id, listener_);
          session->ws_behavior = ws_behavior;

          buffer.erase(buffer.begin(),
                       buffer.begin() + body_start_idx + consumed_body_bytes);

          session->partial_body.clear();
          session->chunk_parsed_up_to = 0;
          session->parsing_chunked = false;
          search_start = 0;

          if (ws_behavior.on_open) {
            ws_behavior.on_open(session->ws_connection);
          }
        } else {
          buffer.erase(buffer.begin(),
                       buffer.begin() + body_start_idx + consumed_body_bytes);

          session->partial_body.clear();
          session->chunk_parsed_up_to = 0;
          session->parsing_chunked = false;
          search_start = 0;

          if (should_close) {
            return;
          }
        }
      } catch (const std::exception &e) {
        HttpResponse response =
            HttpResponse::Plain("500 Internal Server Error", 500);
        response.headers["Connection"] = "close";
        std::string response_data = response.Serialize();
        {
          std::lock_guard<std::mutex> close_lock(close_mutex_);
          sessions_to_close_.insert(session_id);
          pending_send_bytes_[session_id] = response_data.size();
        }
        listener_->Send(session_id, response_data);
        return;
      }
    }
  }

  HttpRequest ParseHeader(const std::string &raw_header) {
    HttpRequest req;
    std::istringstream stream(raw_header);
    std::string line;
    std::vector<std::string> te_encodings;
    bool transfer_encoding_present = false;

    if (std::getline(stream, line)) {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();

      std::istringstream line_stream(line);
      line_stream >> req.method >> req.path >> req.version;
    }

    while (std::getline(stream, line)) {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      if (line.empty())
        break;

      size_t colon_pos = line.find(':');
      if (colon_pos != std::string::npos) {
        std::string key = line.substr(0, colon_pos);
        if (key.find_first_of(" \t") != std::string::npos) {
          req.method.clear();
          return req;
        }
        std::string value = line.substr(colon_pos + 1);

        size_t first_non_space = value.find_first_not_of(" \t\r\n");
        if (first_non_space != std::string::npos) {
          value = value.substr(first_non_space);
        }
        size_t last_non_space = value.find_last_not_of(" \t\r\n");
        if (last_non_space != std::string::npos) {
          value = value.substr(0, last_non_space + 1);
        } else {
          value.clear();
        }

        if (req.headers.find(key) != req.headers.end()) {
          if (CaseInsensitiveEqual()(key, "content-length")) {
            req.method.clear();
            return req;
          }
        }
        if (CaseInsensitiveEqual()(key, "Transfer-Encoding")) {
          transfer_encoding_present = true;
          std::string val = value;
          size_t pos = 0;
          while (true) {
            size_t comma = val.find(',', pos);
            std::string token = (comma == std::string::npos)
                                    ? val.substr(pos)
                                    : val.substr(pos, comma - pos);
            size_t f = token.find_first_not_of(" \t\r\n");
            if (f != std::string::npos) {
              size_t l = token.find_last_not_of(" \t\r\n");
              token = token.substr(f, l - f + 1);
            } else {
              token.clear();
            }
            if (!token.empty()) {
              te_encodings.push_back(token);
            }
            if (comma == std::string::npos)
              break;
            pos = comma + 1;
          }
        }
        req.headers[key] = value;
        req.multi_headers.insert({key, value});
      } else {
        req.method.clear();
        return req;
      }
    }

    if (transfer_encoding_present) {
      if (te_encodings.size() != 1 ||
          !CaseInsensitiveEqual()(te_encodings[0], "chunked")) {
        req.method.clear();
        return req;
      }
    }

    size_t question_mark = req.path.find('?');
    std::string raw_path = req.path;
    std::string query_str;
    if (question_mark != std::string::npos) {
      raw_path = req.path.substr(0, question_mark);
      query_str = req.path.substr(question_mark + 1);
    }

    req.path = url_decode(raw_path, false);

    if (!query_str.empty()) {
      std::istringstream query_stream(query_str);
      std::string pair;
      while (std::getline(query_stream, pair, '&')) {
        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
          std::string key = url_decode(pair.substr(0, eq_pos), true);
          std::string value = url_decode(pair.substr(eq_pos + 1), true);
          req.query_params[key] = value;
        } else if (!pair.empty()) {
          req.query_params[url_decode(pair, true)] = "";
        }
      }
    }

    return req;
  }

  HttpResponse DispatchRequest(HttpRequest &request, uint64_t session_id,
                               bool &upgrade_to_ws,
                               WebSocketBehavior &ws_behavior) {
    upgrade_to_ws = false;

    HttpResponse middleware_res;
    for (auto &middleware : middlewares_) {
      if (!middleware(request, middleware_res)) {
        return middleware_res;
      }
    }

    auto merge_middleware_headers = [&middleware_res](HttpResponse &final_res) {
      for (const auto &[key, value] : middleware_res.headers) {
        if (final_res.headers.find(key) == final_res.headers.end()) {
          final_res.headers[key] = value;
        }
      }
      for (const auto &[key, value] : middleware_res.multi_headers) {
        final_res.multi_headers.insert({key, value});
      }
    };

    bool found_ws_route = false;
    // Note: request.path has already had query parameters stripped from it
    // in ParseHeader (lines 1749-1757) before DispatchRequest is called.
    auto ws_route_it = ws_routes_.find(request.path);
    if (ws_route_it != ws_routes_.end() && request.method == "GET") {
      ws_behavior = ws_route_it->second;
      found_ws_route = true;
    } else if (request.method == "GET") {
      std::vector<std::string> req_segs = split_path(request.path);
      for (const auto &ws_route : ws_routes_list_) {
        std::unordered_map<std::string, std::string> temp_params;
        if (match_route("GET", req_segs, "GET", ws_route.segments,
                        temp_params)) {
          ws_behavior = ws_route.behavior;
          request.path_params = temp_params;
          found_ws_route = true;
          break;
        }
      }
    }

    if (found_ws_route) {
      std::string upgrade_val = request.GetHeader("Upgrade");
      std::string conn_val = request.GetHeader("Connection");
      std::string ws_key = request.GetHeader("Sec-WebSocket-Key");

      std::string lower_upgrade = upgrade_val;
      std::transform(lower_upgrade.begin(), lower_upgrade.end(),
                     lower_upgrade.begin(), safe_tolower);
      std::string lower_conn = conn_val;
      std::transform(lower_conn.begin(), lower_conn.end(), lower_conn.begin(),
                     safe_tolower);

      if (lower_upgrade.find("websocket") != std::string::npos &&
          lower_conn.find("upgrade") != std::string::npos && !ws_key.empty()) {

        size_t first = ws_key.find_first_not_of(" \t\r\n");
        size_t last = ws_key.find_last_not_of(" \t\r\n");
        if (first != std::string::npos && last != std::string::npos) {
          ws_key = ws_key.substr(first, (last - first + 1));
        }

        std::string accept_key = ComputeAcceptKey(ws_key);

        HttpResponse response;
        response.status_code = 101;
        response.status_message = "Switching Protocols";
        response.headers["Upgrade"] = "websocket";
        response.headers["Connection"] = "Upgrade";
        response.headers["Sec-WebSocket-Accept"] = accept_key;

        upgrade_to_ws = true;
        merge_middleware_headers(response);
        return response;
      } else {
        HttpResponse bad_request;
        bad_request.status_code = 400;
        bad_request.status_message = "Bad Request";
        bad_request.headers["Content-Type"] = "text/plain";
        bad_request.body = "Invalid WebSocket upgrade request.";
        merge_middleware_headers(bad_request);
        return bad_request;
      }
    }

    HttpResponse final_response;
    bool found_route = false;

    std::string route_key = request.method + ":" + request.path;
    auto it = routes_.find(route_key);
    if (it != routes_.end()) {
      final_response = it->second(request);
      found_route = true;
    } else {
      std::vector<std::string> req_segs = split_path(request.path);
      for (const auto &route : routes_list_) {
        std::unordered_map<std::string, std::string> temp_params;
        if (match_route(request.method, req_segs, route.method, route.segments,
                        temp_params)) {
          request.path_params = temp_params;
          final_response = route.handler(request);
          found_route = true;
          break;
        }
      }
    }

    if (!found_route && request.method == "HEAD") {
      std::string get_route_key = "GET:" + request.path;
      auto get_it = routes_.find(get_route_key);
      if (get_it != routes_.end()) {
        final_response = get_it->second(request);
        found_route = true;
      } else {
        std::vector<std::string> req_segs = split_path(request.path);
        for (const auto &route : routes_list_) {
          if (route.method == "GET") {
            std::unordered_map<std::string, std::string> temp_params;
            if (match_route("GET", req_segs, "GET", route.segments,
                            temp_params)) {
              request.path_params = temp_params;
              final_response = route.handler(request);
              found_route = true;
              break;
            }
          }
        }
      }
    }

    if (!found_route) {
      final_response.status_code = 404;
      final_response.status_message = "Not Found";
      final_response.headers["Content-Type"] = "text/plain";
      final_response.body = "404 - Route not found.";
    }

    merge_middleware_headers(final_response);
    return final_response;
  }
};

inline std::atomic<size_t> &GetClientWorkerPoolSize() {
  static std::atomic<size_t> size = []() {
    size_t cores = std::thread::hardware_concurrency();
    return cores <= 1 ? 4 : cores;
  }();
  return size;
}

inline void SetClientWorkerPoolSize(size_t size) {
  GetClientWorkerPoolSize().store(size);
}

inline std::shared_ptr<cppasyncworker::WorkerPool> GetClientWorkerPool() {
  static std::shared_ptr<cppasyncworker::WorkerPool> pool =
      std::make_shared<cppasyncworker::WorkerPool>(
          GetClientWorkerPoolSize().load());
  return pool;
}

class HttpClient {
public:
  HttpClient(const std::string &host, uint16_t port,
             std::shared_ptr<cppasyncworker::WorkerPool> pool = nullptr)
      : host_(host), port_(port), pool_(pool ? pool : GetClientWorkerPool()) {}

  HttpClient(const HttpClient &) = delete;
  HttpClient &operator=(const HttpClient &) = delete;
  HttpClient(HttpClient &&) = delete;
  HttpClient &operator=(HttpClient &&) = delete;

  ~HttpClient() {
    {
      std::lock_guard<std::mutex> lock(tasks_mutex_);
      *alive_ = false;
      for (auto &f : pending_tasks_) {
        if (f.valid()) {
          f.wait();
        }
      }
    }
    CleanUpConnection();
  }

  void EnableSSL() { ssl_enabled_ = true; }

  void SetTimeout(std::chrono::milliseconds timeout) { timeout_ = timeout; }
  std::chrono::milliseconds GetTimeout() const { return timeout_; }

  void SetMaxBodySize(size_t size) { max_body_size_ = size; }
  size_t GetMaxBodySize() const { return max_body_size_; }

  void SetMaxHeaderSize(size_t size) { max_header_size_ = size; }
  size_t GetMaxHeaderSize() const { return max_header_size_; }

  void SetMaxRedirects(int max_redirects) { max_redirects_ = max_redirects; }
  int GetMaxRedirects() const { return max_redirects_; }

  HttpResponse
  Get(const std::string &path,
      const std::unordered_map<std::string, std::string> &headers = {}) {
    return SendRequest("GET", path, "", headers);
  }

  HttpResponse
  Post(const std::string &path, const std::string &body,
       const std::unordered_map<std::string, std::string> &headers = {}) {
    return SendRequest("POST", path, body, headers);
  }

  HttpResponse
  Put(const std::string &path, const std::string &body,
      const std::unordered_map<std::string, std::string> &headers = {}) {
    return SendRequest("PUT", path, body, headers);
  }

  HttpResponse
  Delete(const std::string &path,
         const std::unordered_map<std::string, std::string> &headers = {}) {
    return SendRequest("DELETE", path, "", headers);
  }

  std::future<HttpResponse>
  GetAsync(const std::string &path,
           const std::unordered_map<std::string, std::string> &headers = {}) {
    return SendRequestAsync("GET", path, "", headers);
  }

  std::future<HttpResponse>
  PostAsync(const std::string &path, const std::string &body,
            const std::unordered_map<std::string, std::string> &headers = {}) {
    return SendRequestAsync("POST", path, body, headers);
  }

  std::future<HttpResponse>
  PutAsync(const std::string &path, const std::string &body,
           const std::unordered_map<std::string, std::string> &headers = {}) {
    return SendRequestAsync("PUT", path, body, headers);
  }

  std::future<HttpResponse> DeleteAsync(
      const std::string &path,
      const std::unordered_map<std::string, std::string> &headers = {}) {
    return SendRequestAsync("DELETE", path, "", headers);
  }

  HttpResponse
  GetStream(const std::string &path,
            std::function<void(const HttpResponse &, const std::string &, bool)>
                on_chunk,
            const std::unordered_map<std::string, std::string> &headers = {}) {
    std::lock_guard<std::mutex> req_lock(request_mutex_);
    HeaderMap h_map(headers.begin(), headers.end());
    return SendRequestInternal("GET", path, "", h_map, 0, on_chunk);
  }

  HttpResponse PostStream(
      const std::string &path, const std::string &body,
      std::function<void(const HttpResponse &, const std::string &, bool)>
          on_chunk,
      const std::unordered_map<std::string, std::string> &headers = {}) {
    std::lock_guard<std::mutex> req_lock(request_mutex_);
    HeaderMap h_map(headers.begin(), headers.end());
    return SendRequestInternal("POST", path, body, h_map, 0, on_chunk);
  }

  std::future<HttpResponse>
  PostStreamAsync(const std::string &path,
                  const std::unordered_map<std::string, std::string> &headers,
                  std::function<std::string(size_t)> stream_provider,
                  size_t content_length) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);

    pending_tasks_.erase(
        std::remove_if(pending_tasks_.begin(), pending_tasks_.end(),
                       [](const std::shared_future<void> &f) {
                         return f.wait_for(std::chrono::seconds(0)) ==
                                std::future_status::ready;
                       }),
        pending_tasks_.end());

    auto promise = std::make_shared<std::promise<HttpResponse>>();
    auto res_future = promise->get_future();

    HeaderMap h_map(headers.begin(), headers.end());
    std::weak_ptr<bool> alive_weak = alive_;

    auto pool_future =
        pool_->Enqueue([this, alive_weak, promise, path, stream_provider,
                        content_length, h_map]() {
          auto alive = alive_weak.lock();
          if (!alive || !*alive) {
            promise->set_exception(std::make_exception_ptr(
                std::runtime_error("HttpClient destroyed")));
            return;
          }
          try {
            promise->set_value(SendRequestStreamUpload(
                "POST", path, stream_provider, content_length, h_map));
          } catch (...) {
            promise->set_exception(std::current_exception());
          }
        });

    std::shared_future<void> sf(std::move(pool_future));
    pending_tasks_.push_back(sf);

    return res_future;
  }

  std::future<HttpResponse>
  PutStreamAsync(const std::string &path,
                 const std::unordered_map<std::string, std::string> &headers,
                 std::function<std::string(size_t)> stream_provider,
                 size_t content_length) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);

    pending_tasks_.erase(
        std::remove_if(pending_tasks_.begin(), pending_tasks_.end(),
                       [](const std::shared_future<void> &f) {
                         return f.wait_for(std::chrono::seconds(0)) ==
                                std::future_status::ready;
                       }),
        pending_tasks_.end());

    auto promise = std::make_shared<std::promise<HttpResponse>>();
    auto res_future = promise->get_future();

    HeaderMap h_map(headers.begin(), headers.end());
    std::weak_ptr<bool> alive_weak = alive_;

    auto pool_future =
        pool_->Enqueue([this, alive_weak, promise, path, stream_provider,
                        content_length, h_map]() {
          auto alive = alive_weak.lock();
          if (!alive || !*alive) {
            promise->set_exception(std::make_exception_ptr(
                std::runtime_error("HttpClient destroyed")));
            return;
          }
          try {
            promise->set_value(SendRequestStreamUpload(
                "PUT", path, stream_provider, content_length, h_map));
          } catch (...) {
            promise->set_exception(std::current_exception());
          }
        });

    std::shared_future<void> sf(std::move(pool_future));
    pending_tasks_.push_back(sf);

    return res_future;
  }

  std::future<HttpResponse> SendRequestAsync(
      const std::string &method, const std::string &path,
      const std::string &body = "",
      const std::unordered_map<std::string, std::string> &headers = {}) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);

    pending_tasks_.erase(
        std::remove_if(pending_tasks_.begin(), pending_tasks_.end(),
                       [](const std::shared_future<void> &f) {
                         return f.wait_for(std::chrono::seconds(0)) ==
                                std::future_status::ready;
                       }),
        pending_tasks_.end());

    auto promise = std::make_shared<std::promise<HttpResponse>>();
    auto res_future = promise->get_future();

    HeaderMap h_map(headers.begin(), headers.end());
    std::weak_ptr<bool> alive_weak = alive_;

    auto pool_future = pool_->Enqueue(
        [this, alive_weak, promise, method, path, body, h_map]() {
          auto alive = alive_weak.lock();
          if (!alive || !*alive) {
            promise->set_exception(std::make_exception_ptr(
                std::runtime_error("HttpClient destroyed")));
            return;
          }
          try {
            promise->set_value(SendRequest(method, path, body, h_map));
          } catch (...) {
            promise->set_exception(std::current_exception());
          }
        });

    std::shared_future<void> sf(std::move(pool_future));
    pending_tasks_.push_back(sf);

    return res_future;
  }

  HttpResponse SendRequest(
      const std::string &method, const std::string &path,
      const std::string &body = "",
      const std::unordered_map<std::string, std::string> &headers = {}) {
    std::lock_guard<std::mutex> req_lock(request_mutex_);
    HeaderMap h_map(headers.begin(), headers.end());
    return SendRequestInternal(method, path, body, h_map, 0);
  }

  HttpResponse SendRequest(const std::string &method, const std::string &path,
                           const std::string &body, const HeaderMap &headers) {
    std::lock_guard<std::mutex> req_lock(request_mutex_);
    return SendRequestInternal(method, path, body, headers, 0);
  }

private:
  struct ParsedResponseHeader {
    std::string status_code_str;
    std::string status_message;
    HeaderMap headers;
    MultiHeaderMap multi_headers;
    std::string GetHeader(const std::string &key) const {
      auto it = headers.find(key);
      if (it != headers.end()) {
        return it->second;
      }
      return "";
    }
  };

  std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
  std::shared_ptr<cppasyncworker::WorkerPool> pool_;

  std::string host_;
  uint16_t port_;
  bool ssl_enabled_ = false;
  std::chrono::milliseconds timeout_{10000};
  size_t max_body_size_ = 10 * 1024 * 1024;
  size_t max_header_size_ = 8192;
  int max_redirects_ = 5;

  std::unique_ptr<cpptcpnet::TcpClient> client_;
  std::unique_ptr<cpppubsub::Worker> worker_;
  std::shared_ptr<cpppubsub::Subscriber<cpptcpnet::ConnectionEvent>> state_sub_;
  uint64_t session_id_ = 0;
  std::atomic<bool> connected_{false};
  std::atomic<bool> disconnected_{false};
  std::deque<uint8_t> buffer_;
  std::mutex mtx_;
  std::condition_variable cv_;
  std::mutex request_mutex_;

  std::vector<std::shared_future<void>> pending_tasks_;
  std::mutex tasks_mutex_;

  void CleanUpConnection() {
    std::unique_ptr<cpptcpnet::TcpClient> old_client;
    std::unique_ptr<cpppubsub::Worker> old_worker;
    std::shared_ptr<cpppubsub::Subscriber<cpptcpnet::ConnectionEvent>> old_sub;

    {
      std::lock_guard<std::mutex> lock(mtx_);
      old_client = std::move(client_);
      old_worker = std::move(worker_);
      old_sub = std::move(state_sub_);
      session_id_ = 0;
      connected_ = false;
      disconnected_ = false;
      buffer_.clear();
    }

    if (old_client || old_worker) {
      std::thread([c = std::move(old_client), w = std::move(old_worker)]() {
        if (c)
          c->Stop();
        if (w)
          w->Stop();
      }).detach();
    }
  }

  void EnsureConnected() {
    if (connected_ && !disconnected_ && session_id_ != 0) {
      return;
    }

    CleanUpConnection();

    client_ = std::make_unique<cpptcpnet::TcpClient>();
    if (ssl_enabled_) {
      client_->EnableSSL();
    }

    buffer_.clear();
    disconnected_ = false;
    connected_ = false;

    client_->SetDataHandler([this](uint64_t session_id,
                                   const std::vector<uint8_t> &data) {
      std::lock_guard<std::mutex> lock(mtx_);
      if (session_id != session_id_ || session_id == 0) {
        return;
      }

      if (buffer_.size() + data.size() > max_header_size_ + max_body_size_) {
        disconnected_ = true;
        cv_.notify_all();
        return;
      }

      const std::string terminator = "\r\n\r\n";
      auto it = std::search(buffer_.begin(), buffer_.end(), terminator.begin(),
                            terminator.end());
      if (it == buffer_.end() &&
          (buffer_.size() + data.size() > max_header_size_)) {
        disconnected_ = true;
        cv_.notify_all();
        return;
      }

      buffer_.insert(buffer_.end(), data.begin(), data.end());
      cv_.notify_all();
    });

    state_sub_ =
        client_->GetEventBroker().Subscribe<cpptcpnet::ConnectionEvent>(
            "state_events");
    worker_ = std::make_unique<cpppubsub::Worker>();
    worker_->AddSubscription<cpptcpnet::ConnectionEvent>(
        state_sub_, [this](const cpptcpnet::ConnectionEvent &e) {
          std::lock_guard<std::mutex> lock(mtx_);
          if (e.session_id != session_id_ || e.session_id == 0) {
            return;
          }
          if (e.state == cpptcpnet::ConnectionState::Disconnected) {
            disconnected_ = true;
            connected_ = false;
            cv_.notify_all();
          } else if (e.state == cpptcpnet::ConnectionState::Connected) {
            connected_ = true;
            cv_.notify_all();
          }
        });
    worker_->Start();
    client_->Start();

    session_id_ = client_->Connect(host_, port_);
    if (session_id_ == 0) {
      CleanUpConnection();
      throw std::runtime_error("Failed to connect to server");
    }

    std::unique_lock<std::mutex> lock(mtx_);
    if (!cv_.wait_for(lock, timeout_,
                      [&] { return connected_ || disconnected_; })) {
      lock.unlock();
      CleanUpConnection();
      throw std::runtime_error("Connection timeout");
    }
    if (disconnected_ || !connected_) {
      lock.unlock();
      CleanUpConnection();
      throw std::runtime_error("Connection failed during handshake");
    }
  }

  HttpResponse ReadResponse(
      std::unique_lock<std::mutex> &lock, const std::string &method,
      std::function<void(const HttpResponse &, const std::string &, bool)>
          on_chunk) {
    HttpResponse http_response;
    bool response_ready = false;
    size_t consumed_bytes = 0;
    size_t client_chunk_parsed_up_to = 0;
    bool client_parsing_chunked = false;
    size_t client_bytes_received = 0;

    bool header_parsed = false;
    ParsedResponseHeader req;
    size_t body_start_idx = 0;

    auto check_response = [&]() {
      if (response_ready)
        return true;

      if (!header_parsed) {
        const std::string terminator = "\r\n\r\n";
        auto it = std::search(buffer_.begin(), buffer_.end(),
                              terminator.begin(), terminator.end());
        if (it != buffer_.end()) {
          size_t header_end_idx = std::distance(buffer_.begin(), it);
          body_start_idx = header_end_idx + 4;

          std::string raw_header(buffer_.begin(),
                                 buffer_.begin() + header_end_idx);
          req = ParseClientHeader(raw_header);
          header_parsed = true;
        }
      }

      if (header_parsed) {
        int status_code = 0;
        try {
          status_code = std::stoi(req.status_code_str);
        } catch (...) {
        }
        if (method == "HEAD" || status_code == 204 || status_code == 304 ||
            (status_code >= 100 && status_code < 200)) {
          http_response.status_code = status_code;
          http_response.status_message = req.status_message;
          http_response.headers = req.headers;
          http_response.multi_headers = req.multi_headers;
          consumed_bytes = body_start_idx;
          response_ready = true;
          if (on_chunk) {
            on_chunk(http_response, "", true);
          }
          return true;
        }

        std::string te_val = req.GetHeader("Transfer-Encoding");
        std::transform(te_val.begin(), te_val.end(), te_val.begin(),
                       safe_tolower);

        if (te_val.find("chunked") != std::string::npos) {
          bool chunked_bad_request = false;
          bool chunked_payload_too_large = false;
          if (!client_parsing_chunked) {
            client_parsing_chunked = true;
            client_chunk_parsed_up_to = body_start_idx;
            http_response.body.clear();
          }
          bool completed = ParseChunkedBodyIncremental(
              buffer_, client_chunk_parsed_up_to, http_response.body,
              chunked_bad_request, max_body_size_, chunked_payload_too_large,
              [&](const std::string &chunk) {
                if (on_chunk) {
                  HttpResponse temp_res;
                  temp_res.status_code = status_code;
                  temp_res.status_message = req.status_message;
                  temp_res.headers = req.headers;
                  temp_res.multi_headers = req.multi_headers;
                  on_chunk(temp_res, chunk, false);
                }
              });

          if (on_chunk && client_chunk_parsed_up_to > body_start_idx) {
            buffer_.erase(buffer_.begin() + body_start_idx,
                          buffer_.begin() + client_chunk_parsed_up_to);
            client_chunk_parsed_up_to = body_start_idx;
          }

          if (chunked_payload_too_large) {
            disconnected_ = true;
            return true;
          }
          if (chunked_bad_request) {
            disconnected_ = true;
            return true;
          }
          if (completed) {
            try {
              http_response.status_code = std::stoi(req.status_code_str);
            } catch (const std::exception &) {
              throw std::runtime_error("Malformed HTTP response status code: " +
                                       req.status_code_str);
            }
            http_response.status_message = req.status_message;
            http_response.headers = req.headers;
            http_response.multi_headers = req.multi_headers;
            consumed_bytes = client_chunk_parsed_up_to;
            response_ready = true;
            if (on_chunk) {
              on_chunk(http_response, "", true);
            }
            return true;
          }
        } else {
          size_t content_length = 0;
          std::string cl_val = req.GetHeader("Content-Length");
          bool has_cl = !cl_val.empty();
          if (has_cl) {
            try {
              content_length = std::stoull(cl_val);
            } catch (const std::exception &) {
            }
          }

          if (has_cl) {
            if (content_length > max_body_size_ && !on_chunk) {
              disconnected_ = true;
              return true;
            }
            if (on_chunk) {
              size_t available = buffer_.size() - body_start_idx;
              size_t to_process =
                  std::min(available, content_length - client_bytes_received);
              if (to_process > 0) {
                std::string chunk(buffer_.begin() + body_start_idx,
                                  buffer_.begin() + body_start_idx +
                                      to_process);
                HttpResponse temp_res;
                temp_res.status_code = status_code;
                temp_res.status_message = req.status_message;
                temp_res.headers = req.headers;
                temp_res.multi_headers = req.multi_headers;
                on_chunk(temp_res, chunk, false);
                client_bytes_received += to_process;
                buffer_.erase(buffer_.begin() + body_start_idx,
                              buffer_.begin() + body_start_idx + to_process);
              }
              if (client_bytes_received >= content_length) {
                http_response.status_code = status_code;
                http_response.status_message = req.status_message;
                http_response.headers = req.headers;
                http_response.multi_headers = req.multi_headers;
                consumed_bytes = body_start_idx;
                response_ready = true;
                on_chunk(http_response, "", true);
                return true;
              }
            } else {
              if (buffer_.size() - body_start_idx >= content_length) {
                try {
                  http_response.status_code = std::stoi(req.status_code_str);
                } catch (const std::exception &) {
                  throw std::runtime_error(
                      "Malformed HTTP response status code: " +
                      req.status_code_str);
                }
                http_response.status_message = req.status_message;
                http_response.headers = req.headers;
                http_response.multi_headers = req.multi_headers;
                if (content_length > 0) {
                  http_response.body = std::string(
                      buffer_.begin() + body_start_idx,
                      buffer_.begin() + body_start_idx + content_length);
                }
                consumed_bytes = body_start_idx + content_length;
                response_ready = true;
                return true;
              }
            }
          }
        }
      }

      if (disconnected_) {
        if (header_parsed) {
          std::string te_val = req.GetHeader("Transfer-Encoding");
          std::transform(te_val.begin(), te_val.end(), te_val.begin(),
                         safe_tolower);

          std::string cl_val = req.GetHeader("Content-Length");

          if (cl_val.empty() && te_val.find("chunked") == std::string::npos) {
            try {
              http_response.status_code = std::stoi(req.status_code_str);
            } catch (const std::exception &) {
              throw std::runtime_error("Malformed HTTP response status code: " +
                                       req.status_code_str);
            }
            http_response.status_message = req.status_message;
            http_response.headers = req.headers;
            http_response.multi_headers = req.multi_headers;
            if (on_chunk) {
              size_t available = buffer_.size() - body_start_idx;
              if (available > 0) {
                std::string chunk(buffer_.begin() + body_start_idx,
                                  buffer_.end());
                on_chunk(http_response, chunk, false);
              }
              consumed_bytes = buffer_.size();
              response_ready = true;
              on_chunk(http_response, "", true);
              return true;
            } else {
              if (buffer_.size() > body_start_idx) {
                http_response.body = std::string(
                    buffer_.begin() + body_start_idx, buffer_.end());
              }
              consumed_bytes = buffer_.size();
              response_ready = true;
              return true;
            }
          }
        }
        return true;
      }
      return false;
    };

    if (!cv_.wait_for(lock, timeout_, check_response)) {
      lock.unlock();
      CleanUpConnection();
      throw std::runtime_error("Request timed out");
    }

    if (!response_ready) {
      lock.unlock();
      CleanUpConnection();
      throw std::runtime_error(
          "Connection closed before response was fully received");
    }

    buffer_.erase(buffer_.begin(), buffer_.begin() + consumed_bytes);
    lock.unlock();

    bool should_close_connection = false;
    std::string resp_conn = http_response.GetHeader("Connection");
    std::transform(resp_conn.begin(), resp_conn.end(), resp_conn.begin(),
                   safe_tolower);
    if (resp_conn.find("close") != std::string::npos) {
      should_close_connection = true;
    }

    if (should_close_connection) {
      CleanUpConnection();
    }

    std::string content_encoding = http_response.GetHeader("Content-Encoding");
    std::transform(content_encoding.begin(), content_encoding.end(),
                   content_encoding.begin(), safe_tolower);
    if (content_encoding.find("gzip") != std::string::npos &&
        !http_response.body.empty()) {
      auto decompressed = gzip_decompress(http_response.body);
      if (decompressed.has_value()) {
        http_response.body = std::move(*decompressed);
      }
    }

    return http_response;
  }

  HttpResponse
  SendRequestStreamUpload(const std::string &method, const std::string &path,
                          std::function<std::string(size_t)> stream_provider,
                          size_t content_length, const HeaderMap &headers,
                          int redirect_depth = 0) {
    if (redirect_depth > max_redirects_) {
      throw std::runtime_error("Too many redirects");
    }

    if (!is_valid_header_value(method) || !is_valid_header_value(path)) {
      throw std::invalid_argument("Invalid request method or path");
    }

    EnsureConnected();

    auto transfer_sub =
        client_->GetEventBroker().Subscribe<cpptcpnet::TransferEvent>(
            "transfer_events");

    std::ostringstream request_stream;
    request_stream << method << " " << path << " HTTP/1.1\r\n";
    bool is_default_port =
        (ssl_enabled_ && port_ == 443) || (!ssl_enabled_ && port_ == 80);
    if (is_default_port) {
      request_stream << "Host: " << host_ << "\r\n";
    } else {
      request_stream << "Host: " << host_ << ":" << port_ << "\r\n";
    }

    bool is_chunked = (content_length == 0);
    std::string te_val = "";
    for (const auto &[k, v] : headers) {
      std::string lk = k;
      std::transform(lk.begin(), lk.end(), lk.begin(), safe_tolower);
      if (lk == "transfer-encoding")
        te_val = v;
    }
    if (te_val.find("chunked") != std::string::npos) {
      is_chunked = true;
    }

    bool has_connection = false;
    bool has_accept_encoding = false;
    for (const auto &[key, value] : headers) {
      std::string lk = key;
      std::transform(lk.begin(), lk.end(), lk.begin(), safe_tolower);
      if (lk == "connection")
        has_connection = true;
      if (lk == "accept-encoding")
        has_accept_encoding = true;
      if (lk == "transfer-encoding")
        continue;
      request_stream << key << ": " << value << "\r\n";
    }

    if (!has_connection) {
      request_stream << "Connection: keep-alive\r\n";
    }
    if (!has_accept_encoding) {
      request_stream << "Accept-Encoding: gzip\r\n";
    }

    if (is_chunked) {
      request_stream << "Transfer-Encoding: chunked\r\n";
    } else {
      request_stream << "Content-Length: " << content_length << "\r\n";
    }
    request_stream << "\r\n";

    if (!client_->Send(session_id_, request_stream.str())) {
      CleanUpConnection();
      throw std::runtime_error("Failed to send request headers");
    }

    size_t total_sent = 0;
    size_t client_bytes_in_flight = 0;
    size_t chunk_size = 65536;

    while (is_chunked || total_sent < content_length) {
      if (!*alive_) {
        throw std::runtime_error("HttpClient destroyed");
      }
      while (auto event = transfer_sub->try_receive()) {
        if (event->is_send && event->session_id == session_id_) {
          if (client_bytes_in_flight >= event->bytes_transferred) {
            client_bytes_in_flight -= event->bytes_transferred;
          } else {
            client_bytes_in_flight = 0;
          }
        }
      }

      size_t max_flight =
          client_->GetConnectionProfile(session_id_).max_outbound_buffer_size /
          2;
      if (max_flight < 65536)
        max_flight = 65536;

      if (client_bytes_in_flight >= max_flight) {
#ifdef _WIN32
        WaitForSingleObject(transfer_sub->GetWaitHandle(), 5);
#else
        struct pollfd pfd;
        pfd.fd = transfer_sub->GetWaitFD();
        pfd.events = POLLIN;
        poll(&pfd, 1, 5);
#endif
        continue;
      }

      size_t ask_size = chunk_size;
      if (!is_chunked) {
        ask_size = std::min(chunk_size, content_length - total_sent);
      }
      std::string chunk = stream_provider(ask_size);

      if (chunk.empty()) {
        if (is_chunked) {
          std::string final_chunk = "0\r\n\r\n";
          client_->Send(session_id_, final_chunk);
        }
        break;
      }

      std::string payload_to_send;
      if (is_chunked) {
        std::ostringstream chunk_stream;
        chunk_stream << std::hex << chunk.size() << "\r\n" << chunk << "\r\n";
        payload_to_send = chunk_stream.str();
      } else {
        payload_to_send = std::move(chunk);
      }

      size_t payload_bytes = payload_to_send.size();
      if (client_->Send(session_id_, payload_to_send)) {
        client_bytes_in_flight += payload_bytes;
        total_sent += chunk.size();
      } else {
        CleanUpConnection();
        throw std::runtime_error("Failed to send chunk");
      }
    }

    std::unique_lock<std::mutex> lock(mtx_);
    HttpResponse http_response = ReadResponse(lock, method, nullptr);

    // Follow redirects
    if (http_response.status_code >= 300 && http_response.status_code < 400) {
      std::string location = http_response.headers["Location"];
      if (!location.empty()) {
        std::string next_method = method;
        if (http_response.status_code == 301 ||
            http_response.status_code == 302 ||
            http_response.status_code == 303) {
          next_method = "GET";
        }

        if (location[0] == '/') {
          if (next_method == "GET") {
            return SendRequestInternal(next_method, location, "", headers,
                                       redirect_depth + 1);
          } else {
            return SendRequestStreamUpload(next_method, location,
                                           stream_provider, content_length,
                                           headers, redirect_depth + 1);
          }
        } else {
          std::string r_proto, r_host, r_path;
          uint16_t r_port;
          if (parse_url(location, r_proto, r_host, r_port, r_path)) {
            if (r_host == host_ && r_port == port_ &&
                (r_proto == "https") == ssl_enabled_) {
              if (next_method == "GET") {
                return SendRequestInternal(next_method, r_path, "", headers,
                                           redirect_depth + 1);
              } else {
                return SendRequestStreamUpload(next_method, r_path,
                                               stream_provider, content_length,
                                               headers, redirect_depth + 1);
              }
            } else {
              HttpClient new_client(r_host, r_port);
              if (r_proto == "https") {
                new_client.EnableSSL();
              }
              new_client.SetTimeout(timeout_);
              new_client.SetMaxBodySize(max_body_size_);
              new_client.SetMaxHeaderSize(max_header_size_);
              new_client.SetMaxRedirects(max_redirects_);
              HeaderMap next_headers = headers;
              next_headers.erase("authorization");
              if (next_method == "GET") {
                return new_client.SendRequestInternal(
                    next_method, r_path, "", next_headers, redirect_depth + 1);
              } else {
                return new_client.SendRequestStreamUpload(
                    next_method, r_path, stream_provider, content_length,
                    next_headers, redirect_depth + 1);
              }
            }
          }
        }
      }
    }

    return http_response;
  }

  HttpResponse SendRequestInternal(
      const std::string &method, const std::string &path,
      const std::string &body, const HeaderMap &headers, int redirect_depth,
      std::function<void(const HttpResponse &, const std::string &, bool)>
          on_chunk = nullptr) {
    if (redirect_depth > max_redirects_) {
      throw std::runtime_error("Too many redirects");
    }

    if (!is_valid_header_value(method)) {
      throw std::invalid_argument("Invalid request method (contains CRLF)");
    }

    if (!is_valid_header_value(path)) {
      throw std::invalid_argument("Invalid request path (contains CRLF)");
    }

    for (const auto &[key, value] : headers) {
      if (!is_valid_header_value(key) || !is_valid_header_value(value)) {
        throw std::invalid_argument(
            "Invalid header key or value (contains CRLF)");
      }
    }

    EnsureConnected();

    std::ostringstream request_stream;
    request_stream << method << " " << path << " HTTP/1.1\r\n";
    bool is_default_port =
        (ssl_enabled_ && port_ == 443) || (!ssl_enabled_ && port_ == 80);
    if (is_default_port) {
      request_stream << "Host: " << host_ << "\r\n";
    } else {
      request_stream << "Host: " << host_ << ":" << port_ << "\r\n";
    }

    bool has_connection = (headers.find("connection") != headers.end());
    bool has_accept_encoding =
        (headers.find("accept-encoding") != headers.end());
    for (const auto &[key, value] : headers) {
      request_stream << key << ": " << value << "\r\n";
    }

    if (!has_connection) {
      request_stream << "Connection: keep-alive\r\n";
    }
    if (!has_accept_encoding) {
      request_stream << "Accept-Encoding: gzip\r\n";
    }

    if (!body.empty()) {
      request_stream << "Content-Length: " << body.length() << "\r\n";
    } else {
      std::string lower_method = method;
      std::transform(lower_method.begin(), lower_method.end(),
                     lower_method.begin(), safe_tolower);
      if (lower_method == "post" || lower_method == "put" ||
          lower_method == "patch") {
        bool has_content_length =
            (headers.find("content-length") != headers.end());
        bool has_transfer_encoding =
            (headers.find("transfer-encoding") != headers.end());
        if (!has_content_length && !has_transfer_encoding) {
          request_stream << "Content-Length: 0\r\n";
        }
      }
    }
    request_stream << "\r\n";
    if (!body.empty()) {
      request_stream << body;
    }

    if (!client_->Send(session_id_, request_stream.str())) {
      CleanUpConnection();
      throw std::runtime_error("Failed to send request");
    }

    std::unique_lock<std::mutex> lock(mtx_);
    HttpResponse http_response = ReadResponse(lock, method, on_chunk);

    // Follow redirects
    if (http_response.status_code >= 300 && http_response.status_code < 400) {
      std::string location = http_response.headers["Location"];
      if (!location.empty()) {
        std::string next_method = method;
        std::string next_body = body;
        if (http_response.status_code == 301 ||
            http_response.status_code == 302 ||
            http_response.status_code == 303) {
          next_method = "GET";
          next_body = "";
        }

        if (location[0] == '/') {
          return SendRequestInternal(next_method, location, next_body, headers,
                                     redirect_depth + 1, on_chunk);
        } else {
          std::string r_proto, r_host, r_path;
          uint16_t r_port;
          if (parse_url(location, r_proto, r_host, r_port, r_path)) {
            if (r_host == host_ && r_port == port_ &&
                (r_proto == "https") == ssl_enabled_) {
              return SendRequestInternal(next_method, r_path, next_body,
                                         headers, redirect_depth + 1, on_chunk);
            } else {
              HttpClient new_client(r_host, r_port);
              if (r_proto == "https") {
                new_client.EnableSSL();
              }
              new_client.SetTimeout(timeout_);
              new_client.SetMaxBodySize(max_body_size_);
              new_client.SetMaxHeaderSize(max_header_size_);
              new_client.SetMaxRedirects(max_redirects_);
              HeaderMap next_headers = headers;
              next_headers.erase("authorization");
              return new_client.SendRequestInternal(
                  next_method, r_path, next_body, next_headers,
                  redirect_depth + 1, on_chunk);
            }
          }
        }
      }
    }

    return http_response;
  }

  ParsedResponseHeader ParseClientHeader(const std::string &raw_header) {
    ParsedResponseHeader req;
    std::istringstream stream(raw_header);
    std::string line;

    if (std::getline(stream, line)) {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();

      std::istringstream line_stream(line);
      std::string protocol;
      std::string status_code;
      std::string status_message;

      line_stream >> protocol >> status_code;
      std::getline(line_stream, status_message);

      if (!status_message.empty() && status_message.front() == ' ') {
        status_message = status_message.substr(1);
      }

      req.status_code_str = status_code;
      req.status_message = status_message;
    }

    while (std::getline(stream, line)) {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      if (line.empty())
        break;

      size_t colon_pos = line.find(':');
      if (colon_pos != std::string::npos) {
        std::string key = line.substr(0, colon_pos);
        std::string value = line.substr(colon_pos + 1);

        size_t first_non_space = value.find_first_not_of(" \t\r\n");
        if (first_non_space != std::string::npos) {
          value = value.substr(first_non_space);
        }
        size_t last_non_space = value.find_last_not_of(" \t\r\n");
        if (last_non_space != std::string::npos) {
          value = value.substr(0, last_non_space + 1);
        } else {
          value.clear();
        }

        req.headers[key] = value;
        req.multi_headers.insert({key, value});
      }
    }

    return req;
  }
};

class WebSocketClient {
public:
  using WsConnectHandler = std::function<void()>;
  using WsMessageHandler = std::function<void(const std::string &)>;
  using WsBinaryHandler = std::function<void(const std::vector<uint8_t> &)>;
  using WsCloseHandler = std::function<void(uint16_t, const std::string &)>;

  WebSocketClient() = default;
  WebSocketClient(const WebSocketClient &) = delete;
  WebSocketClient &operator=(const WebSocketClient &) = delete;
  WebSocketClient(WebSocketClient &&) = delete;
  WebSocketClient &operator=(WebSocketClient &&) = delete;
  ~WebSocketClient() { Disconnect(); }

  void OnOpen(WsConnectHandler handler) { on_open_ = std::move(handler); }
  void OnMessage(WsMessageHandler handler) { on_message_ = std::move(handler); }
  void OnBinary(WsBinaryHandler handler) { on_binary_ = std::move(handler); }
  void OnClose(WsCloseHandler handler) { on_close_ = std::move(handler); }

  void SetMaxBodySize(size_t size) { max_body_size_ = size; }
  size_t GetMaxBodySize() const { return max_body_size_; }

  void SetCloseTimeout(std::chrono::milliseconds timeout) {
    close_timeout_ = timeout;
  }
  std::chrono::milliseconds GetCloseTimeout() const { return close_timeout_; }

  bool Connect(const std::string &url,
               std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
    std::string protocol, host, path;
    uint16_t port;
    if (!parse_url(url, protocol, host, port, path)) {
      return false;
    }

    ssl_enabled_ = (protocol == "https" || protocol == "wss");

    client_ = std::make_unique<cpptcpnet::TcpClient>();
    if (ssl_enabled_) {
      client_->EnableSSL();
    }

    buffer_.clear();
    connected_ = false;
    disconnected_ = false;
    handshake_completed_ = false;

    client_->SetDataHandler(
        [this](uint64_t /*session_id*/, const std::vector<uint8_t> &data) {
          bool run_process_frames = false;
          {
            std::lock_guard<std::mutex> lock(mtx_);
            buffer_.insert(buffer_.end(), data.begin(), data.end());
            cv_.notify_all();
            if (handshake_completed_) {
              run_process_frames = true;
            }
          }
          if (run_process_frames) {
            ProcessFrames();
          }
        });

    state_sub_ =
        client_->GetEventBroker().Subscribe<cpptcpnet::ConnectionEvent>(
            "state_events");
    worker_ = std::make_unique<cpppubsub::Worker>();
    worker_->AddSubscription<cpptcpnet::ConnectionEvent>(
        state_sub_, [this](const cpptcpnet::ConnectionEvent &e) {
          std::lock_guard<std::mutex> lock(mtx_);
          if (e.state == cpptcpnet::ConnectionState::Disconnected) {
            disconnected_ = true;
            connected_ = false;
            cv_.notify_all();
          } else if (e.state == cpptcpnet::ConnectionState::Connected) {
            connected_ = true;
            cv_.notify_all();
          }
        });
    worker_->Start();
    client_->Start();

    session_id_ = client_->Connect(host, port);
    if (session_id_ == 0) {
      Disconnect();
      return false;
    }

    std::unique_lock<std::mutex> lock(mtx_);
    if (!cv_.wait_for(lock, timeout,
                      [this] { return connected_ || disconnected_; })) {
      lock.unlock();
      Disconnect();
      return false;
    }
    if (disconnected_ || !connected_) {
      lock.unlock();
      Disconnect();
      return false;
    }

    // Perform Handshake
    std::string key_bytes(16, '\0');
    {
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<uint32_t> dis(0, 255);
      for (int i = 0; i < 16; ++i) {
        key_bytes[i] = static_cast<char>(dis(gen));
      }
    }
    std::string key = base64_encode(key_bytes);
    std::string expected_accept = ComputeAcceptKey(key);

    std::ostringstream handshake_req;
    handshake_req << "GET " << path << " HTTP/1.1\r\n";
    bool is_default_port =
        (ssl_enabled_ && port == 443) || (!ssl_enabled_ && port == 80);
    if (is_default_port) {
      handshake_req << "Host: " << host << "\r\n";
    } else {
      handshake_req << "Host: " << host << ":" << port << "\r\n";
    }
    handshake_req << "Upgrade: websocket\r\n";
    handshake_req << "Connection: Upgrade\r\n";
    handshake_req << "Sec-WebSocket-Key: " << key << "\r\n";
    handshake_req << "Sec-WebSocket-Version: 13\r\n\r\n";

    if (!client_->Send(session_id_, handshake_req.str())) {
      lock.unlock();
      Disconnect();
      return false;
    }

    auto check_handshake = [this]() {
      const std::string terminator = "\r\n\r\n";
      auto it = std::search(buffer_.begin(), buffer_.end(), terminator.begin(),
                            terminator.end());
      return it != buffer_.end();
    };

    if (!cv_.wait_for(lock, timeout, check_handshake)) {
      lock.unlock();
      Disconnect();
      return false;
    }

    const std::string terminator = "\r\n\r\n";
    auto it = std::search(buffer_.begin(), buffer_.end(), terminator.begin(),
                          terminator.end());
    size_t terminator_idx = std::distance(buffer_.begin(), it);
    std::string handshake_res(buffer_.begin(),
                              buffer_.begin() + terminator_idx);
    buffer_.erase(buffer_.begin(), buffer_.begin() + terminator_idx + 4);

    if (handshake_res.find("101 Switching Protocols") == std::string::npos) {
      lock.unlock();
      Disconnect();
      return false;
    }

    // Parse response headers to validate Sec-WebSocket-Accept
    HeaderMap response_headers;
    std::istringstream stream(handshake_res);
    std::string line;
    // Skip status line
    std::getline(stream, line);
    while (std::getline(stream, line)) {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      if (line.empty())
        break;
      size_t colon_pos = line.find(':');
      if (colon_pos != std::string::npos) {
        std::string h_key = line.substr(0, colon_pos);
        std::string h_val = line.substr(colon_pos + 1);
        size_t first = h_val.find_first_not_of(" \t\r\n");
        size_t last = h_val.find_last_not_of(" \t\r\n");
        if (first != std::string::npos && last != std::string::npos) {
          h_val = h_val.substr(first, last - first + 1);
        } else {
          h_val.clear();
        }
        response_headers[h_key] = h_val;
      }
    }

    auto accept_it = response_headers.find("Sec-WebSocket-Accept");
    if (accept_it == response_headers.end() ||
        accept_it->second != expected_accept) {
      lock.unlock();
      Disconnect();
      return false;
    }

    handshake_completed_ = true;
    lock.unlock();
    if (on_open_) {
      on_open_();
    }

    ProcessFrames();

    return true;
  }

  void Disconnect() {
    std::unique_ptr<cpptcpnet::TcpClient> old_client;
    std::unique_ptr<cpppubsub::Worker> old_worker;
    std::shared_ptr<cpppubsub::Subscriber<cpptcpnet::ConnectionEvent>> old_sub;

    {
      std::lock_guard<std::mutex> lock(mtx_);
      old_client = std::move(client_);
      old_worker = std::move(worker_);
      old_sub = std::move(state_sub_);
      session_id_ = 0;
      connected_ = false;
      disconnected_ = false;
      handshake_completed_ = false;
      buffer_.clear();
      cv_.notify_all();
    }

    if (old_client || old_worker) {
      std::thread([c = std::move(old_client), w = std::move(old_worker)]() {
        if (c)
          c->Stop();
        if (w)
          w->Stop();
      }).detach();
    }
  }

  bool Send(const std::string &message) {
    return SendFrame(0x01,
                     std::vector<uint8_t>(message.begin(), message.end()));
  }

  bool SendBinary(const std::vector<uint8_t> &data) {
    return SendFrame(0x02, data);
  }

  bool Ping(const std::string &payload = "") {
    return SendFrame(0x09,
                     std::vector<uint8_t>(payload.begin(), payload.end()));
  }

  void Close(uint16_t status_code = 1000, const std::string &reason = "") {
    std::vector<uint8_t> payload;
    payload.push_back(static_cast<uint8_t>((status_code >> 8) & 0xFF));
    payload.push_back(static_cast<uint8_t>(status_code & 0xFF));
    payload.insert(payload.end(), reason.begin(), reason.end());

    {
      std::lock_guard<std::mutex> lock(mtx_);
      if (!connected_)
        return;
    }

    SendFrame(0x08, payload);

    if (status_code == 1000) {
      std::unique_lock<std::mutex> lock(mtx_);
      cv_.wait_for(lock, close_timeout_, [this]() { return !connected_; });
    }

    std::unique_lock<std::mutex> lock(mtx_);
    if (connected_) {
      lock.unlock();
      Disconnect();
    }
  }

private:
  bool SendFrame(uint8_t opcode, const std::vector<uint8_t> &payload) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!connected_ || session_id_ == 0 || !client_)
      return false;

    std::vector<uint8_t> frame;
    frame.push_back(0x80 | (opcode & 0x0F));

    uint64_t len = payload.size();
    uint8_t len_byte = 0x80; // client MUST mask
    if (len <= 125) {
      frame.push_back(len_byte | static_cast<uint8_t>(len));
    } else if (len <= 65535) {
      frame.push_back(len_byte | 126);
      frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
      frame.push_back(static_cast<uint8_t>(len & 0xFF));
    } else {
      frame.push_back(len_byte | 127);
      for (int i = 7; i >= 0; --i) {
        frame.push_back(static_cast<uint8_t>((len >> (i * 8)) & 0xFF));
      }
    }

    uint8_t mask[4];
    thread_local std::mt19937 gen([]() {
      std::random_device rd;
      return std::mt19937(rd());
    }());
    std::uniform_int_distribution<uint32_t> dis(0, 255);
    for (int i = 0; i < 4; ++i) {
      mask[i] = static_cast<uint8_t>(dis(gen));
    }
    frame.insert(frame.end(), mask, mask + 4);

    for (size_t i = 0; i < len; ++i) {
      frame.push_back(payload[i] ^ mask[i % 4]);
    }

    return client_->Send(session_id_, frame);
  }

  void ProcessFrames() {
    {
      std::lock_guard<std::mutex> lock(mtx_);
      if (processing_frames_) {
        return;
      }
      processing_frames_ = true;
    }

    struct Cleanup {
      WebSocketClient *self;
      ~Cleanup() {
        std::lock_guard<std::mutex> lock(self->mtx_);
        self->processing_frames_ = false;
      }
    } cleanup{this};

    while (true) {
      WsFrame frame;
      bool protocol_error = false;
      bool payload_too_big = false;
      bool parsed = false;
      size_t size_before = 0;

      {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!connected_) {
          return;
        }
        size_before = buffer_.size();
        parsed = ParseWsFrame(buffer_, frame, protocol_error, payload_too_big,
                              false, max_body_size_);
      }

      if (protocol_error) {
        if (on_close_) {
          on_close_(1002, "Protocol error");
        }
        Close(1002, "Protocol error");
        return;
      }
      if (payload_too_big) {
        if (on_close_) {
          on_close_(1009, "Message too big");
        }
        Close(1009, "Message too big");
        return;
      }
      if (parsed) {
        if (frame.opcode == 0x08) {
          uint16_t status_code = 1000;
          std::string reason;
          if (frame.payload.size() >= 2) {
            status_code = (frame.payload[0] << 8) | frame.payload[1];
            reason =
                std::string(frame.payload.begin() + 2, frame.payload.end());
          }
          if (on_close_) {
            on_close_(status_code, reason);
          }
          Disconnect();
          return;
        } else if (frame.opcode == 0x09) {
          SendFrame(0x0A, frame.payload);
        } else if (frame.opcode == 0x0A) {
          // Pong received
        } else if (frame.opcode == 0x01 || frame.opcode == 0x02 ||
                   frame.opcode == 0x00) {
          bool is_protocol_error = false;
          bool is_too_big = false;
          bool trigger_message = false;
          bool trigger_binary = false;
          std::vector<uint8_t> local_assembled_payload;

          {
            std::lock_guard<std::mutex> lock(mtx_);
            if (!connected_) {
              return;
            }
            if (frame.opcode != 0x00) {
              if (ws_fragmented_) {
                is_protocol_error = true;
              } else {
                ws_fragmented_opcode_ = frame.opcode;
                ws_fragmented_ = !frame.fin;
                ws_assembled_payload_ = std::move(frame.payload);
              }
            } else {
              if (!ws_fragmented_) {
                is_protocol_error = true;
              } else {
                ws_assembled_payload_.insert(ws_assembled_payload_.end(),
                                             frame.payload.begin(),
                                             frame.payload.end());
                ws_fragmented_ = !frame.fin;
              }
            }

            if (!is_protocol_error &&
                ws_assembled_payload_.size() > max_body_size_) {
              is_too_big = true;
            }

            if (!is_protocol_error && !is_too_big && frame.fin) {
              uint8_t final_opcode = ws_fragmented_opcode_;
              ws_fragmented_ = false;
              ws_fragmented_opcode_ = 0;

              if (final_opcode == 0x01) {
                trigger_message = true;
                local_assembled_payload = std::move(ws_assembled_payload_);
              } else if (final_opcode == 0x02) {
                trigger_binary = true;
                local_assembled_payload = std::move(ws_assembled_payload_);
              }
              ws_assembled_payload_.clear();
            }
          }

          if (is_protocol_error) {
            Close(1002, "Protocol error");
            return;
          }
          if (is_too_big) {
            Close(1009, "Message too big");
            return;
          }

          if (trigger_message && on_message_) {
            std::string text(local_assembled_payload.begin(),
                             local_assembled_payload.end());
            on_message_(text);
          } else if (trigger_binary && on_binary_) {
            on_binary_(local_assembled_payload);
          }
        }
      } else {
        std::lock_guard<std::mutex> lock(mtx_);
        if (buffer_.size() == size_before) {
          break;
        }
      }
    }
  }

  WsConnectHandler on_open_;
  WsMessageHandler on_message_;
  WsBinaryHandler on_binary_;
  WsCloseHandler on_close_;

  std::unique_ptr<cpptcpnet::TcpClient> client_;
  std::unique_ptr<cpppubsub::Worker> worker_;
  std::shared_ptr<cpppubsub::Subscriber<cpptcpnet::ConnectionEvent>> state_sub_;
  uint64_t session_id_ = 0;
  std::atomic<bool> connected_{false};
  std::atomic<bool> disconnected_{false};
  std::atomic<bool> handshake_completed_{false};
  std::deque<uint8_t> buffer_;
  std::mutex mtx_;
  std::condition_variable cv_;
  bool ssl_enabled_ = false;
  size_t max_body_size_ = 10 * 1024 * 1024;
  std::chrono::milliseconds close_timeout_{1000};

  bool ws_fragmented_ = false;
  uint8_t ws_fragmented_opcode_ = 0;
  std::vector<uint8_t> ws_assembled_payload_;
  bool processing_frames_ = false;
};

} // namespace cpphttp