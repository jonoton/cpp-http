#pragma once

#include <brotli/decode.h>
#include <brotli/encode.h>
#include <openssl/evp.h>
#include <zlib.h>
#include <zstd.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "cppasyncworker.hpp"
#include "cpptcpnet.hpp"
#ifdef CPPHTTP_ENABLE_QUIC
#include "cppquic.hpp"
#endif

namespace cpphttp {

constexpr int VERSION_MAJOR = 1;
constexpr int VERSION_MINOR = 9;
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

enum class LogSeverity { Info, Warning, Error };

using LogCallback =
    std::function<void(LogSeverity severity, const std::string &className,
                       const std::string &message)>;

namespace internal {
inline std::mutex &GetLoggerMutex() {
  static std::mutex mtx;
  return mtx;
}

inline LogCallback &GetLogger() {
  static LogCallback logger = nullptr;
  return logger;
}

inline void Log(LogSeverity severity, const std::string &className,
                const std::string &message) {
  LogCallback handler;
  {
    std::lock_guard<std::mutex> lock(GetLoggerMutex());
    handler = GetLogger();
  }
  if (handler) {
    handler(severity, className, message);
  }
}
}  // namespace internal

inline void SetLogger(LogCallback logger) {
  std::lock_guard<std::mutex> lock(internal::GetLoggerMutex());
  internal::GetLogger() = std::move(logger);
}

namespace qpack {

struct StaticEntry {
  std::string name;
  std::string value;
};

inline const std::vector<StaticEntry> &GetStaticTable() {
  static const std::vector<StaticEntry> static_table = {
      {":authority", ""},                                     // 0
      {":path", "/"},                                         // 1
      {"age", "0"},                                           // 2
      {"content-disposition", ""},                            // 3
      {"content-length", "0"},                                // 4
      {"cookie", ""},                                         // 5
      {"date", ""},                                           // 6
      {"etag", ""},                                           // 7
      {"if-modified-since", ""},                              // 8
      {"if-none-match", ""},                                  // 9
      {"last-modified", ""},                                  // 10
      {"link", ""},                                           // 11
      {"location", ""},                                       // 12
      {"referer", ""},                                        // 13
      {"set-cookie", ""},                                     // 14
      {":method", "CONNECT"},                                 // 15
      {":method", "DELETE"},                                  // 16
      {":method", "GET"},                                     // 17
      {":method", "HEAD"},                                    // 18
      {":method", "OPTIONS"},                                 // 19
      {":method", "POST"},                                    // 20
      {":method", "PUT"},                                     // 21
      {":scheme", "http"},                                    // 22
      {":scheme", "https"},                                   // 23
      {":status", "103"},                                     // 24
      {":status", "200"},                                     // 25
      {":status", "304"},                                     // 26
      {":status", "404"},                                     // 27
      {":status", "503"},                                     // 28
      {"accept", "*/*"},                                      // 29
      {"accept", "application/dns-message"},                  // 30
      {"accept-encoding", "gzip, deflate, br"},               // 31
      {"accept-ranges", "bytes"},                             // 32
      {"access-control-allow-headers", "cache-control"},      // 33
      {"access-control-allow-headers", "content-type"},       // 34
      {"access-control-allow-origin", "*"},                   // 35
      {"cache-control", "max-age=0"},                         // 36
      {"cache-control", "max-age=2592000"},                   // 37
      {"cache-control", "max-age=604800"},                    // 38
      {"cache-control", "no-cache"},                          // 39
      {"cache-control", "no-store"},                          // 40
      {"cache-control", "public, max-age=31536000"},          // 41
      {"content-encoding", "br"},                             // 42
      {"content-encoding", "gzip"},                           // 43
      {"content-type", "application/dns-message"},            // 44
      {"content-type", "application/javascript"},             // 45
      {"content-type", "application/json"},                   // 46
      {"content-type", "application/x-www-form-urlencoded"},  // 47
      {"content-type", "image/gif"},                          // 48
      {"content-type", "image/jpeg"},                         // 49
      {"content-type", "image/png"},                          // 50
      {"content-type", "text/css"},                           // 51
      {"content-type", "text/html; charset=utf-8"},           // 52
      {"content-type", "text/plain"},                         // 53
      {"content-type", "text/plain;charset=utf-8"},           // 54
      {"range", "bytes=0-"},                                  // 55
      {"strict-transport-security", "max-age=31536000"},      // 56
      {"strict-transport-security",
       "max-age=31536000; includesubdomains"},  // 57
      {"strict-transport-security",
       "max-age=31536000; includesubdomains; preload"},        // 58
      {"vary", "accept-encoding"},                             // 59
      {"vary", "origin"},                                      // 60
      {"x-content-type-options", "nosniff"},                   // 61
      {"x-xss-protection", "1; mode=block"},                   // 62
      {":status", "100"},                                      // 63
      {":status", "204"},                                      // 64
      {":status", "206"},                                      // 65
      {":status", "302"},                                      // 66
      {":status", "400"},                                      // 67
      {":status", "403"},                                      // 68
      {":status", "421"},                                      // 69
      {":status", "425"},                                      // 70
      {":status", "500"},                                      // 71
      {"accept-language", ""},                                 // 72
      {"access-control-allow-credentials", "FALSE"},           // 73
      {"access-control-allow-credentials", "TRUE"},            // 74
      {"access-control-allow-headers", "*"},                   // 75
      {"access-control-allow-methods", "get"},                 // 76
      {"access-control-allow-methods", "get, post, options"},  // 77
      {"access-control-allow-methods", "options"},             // 78
      {"access-control-expose-headers", "content-length"},     // 79
      {"access-control-request-headers", "content-type"},      // 80
      {"access-control-request-method", "get"},                // 81
      {"access-control-request-method", "post"},               // 82
      {"alt-svc", "clear"},                                    // 83
      {"authorization", ""},                                   // 84
      {"content-security-policy",
       "script-src 'none'; object-src 'none'; base-uri 'none'"},  // 85
      {"early-data", "1"},                                        // 86
      {"expect-ct", ""},                                          // 87
      {"forwarded", ""},                                          // 88
      {"if-range", ""},                                           // 89
      {"origin", ""},                                             // 90
      {"purpose", "prefetch"},                                    // 91
      {"server", ""},                                             // 92
      {"timing-allow-origin", "*"},                               // 93
      {"upgrade-insecure-requests", "1"},                         // 94
      {"user-agent", ""},                                         // 95
      {"x-forwarded-for", ""},                                    // 96
      {"x-frame-options", "deny"},                                // 97
      {"x-frame-options", "sameorigin"}                           // 98
  };
  return static_table;
}

struct HuffmanNode {
  int symbol = -1;
  int child[2] = {-1, -1};
};

inline const std::vector<HuffmanNode> &GetHuffmanTree() {
  static std::vector<HuffmanNode> tree;
  static std::once_flag once;
  std::call_once(once, []() {
    static const uint8_t code_lengths[257] = {
        13, 23, 28, 28, 28, 28, 28, 28, 28, 24, 30, 28, 28, 30, 28, 28, 28, 28,
        28, 28, 28, 28, 30, 28, 28, 28, 28, 28, 28, 28, 28, 28, 6,  10, 10, 12,
        13, 6,  8,  11, 10, 10, 8,  11, 8,  6,  6,  6,  5,  5,  5,  6,  6,  6,
        6,  6,  6,  6,  7,  8,  15, 6,  12, 10, 13, 6,  7,  7,  7,  7,  7,  7,
        7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  8,  7,
        8,  13, 19, 13, 14, 6,  15, 5,  6,  5,  6,  5,  6,  6,  6,  5,  7,  7,
        6,  6,  6,  5,  6,  7,  6,  5,  5,  6,  7,  7,  7,  7,  7,  15, 11, 14,
        13, 28, 20, 22, 20, 20, 22, 22, 22, 23, 22, 23, 23, 23, 23, 23, 24, 23,
        24, 24, 22, 23, 24, 23, 23, 23, 23, 21, 22, 23, 22, 23, 23, 24, 22, 21,
        20, 22, 22, 23, 23, 21, 23, 22, 22, 24, 21, 22, 23, 23, 21, 21, 22, 21,
        23, 22, 23, 23, 20, 22, 22, 22, 23, 22, 22, 23, 26, 26, 20, 19, 22, 23,
        22, 25, 26, 26, 26, 27, 27, 26, 24, 25, 19, 21, 26, 27, 27, 26, 27, 24,
        21, 21, 26, 26, 28, 27, 27, 27, 20, 24, 20, 21, 22, 21, 21, 23, 22, 22,
        25, 25, 24, 24, 26, 23, 26, 27, 26, 26, 27, 27, 27, 27, 27, 28, 27, 27,
        27, 27, 27, 26, 30};
    static const uint32_t right_codes[257] = {
        0b00000000000000000001111111111000,  // 0x00
        0b00000000011111111111111111011000,  // 0x01
        0b00001111111111111111111111100010,  // 0x02
        0b00001111111111111111111111100011,  // 0x03
        0b00001111111111111111111111100100,  // 0x04
        0b00001111111111111111111111100101,  // 0x05
        0b00001111111111111111111111100110,  // 0x06
        0b00001111111111111111111111100111,  // 0x07
        0b00001111111111111111111111101000,  // 0x08
        0b00000000111111111111111111101010,  // 0x09
        0b00111111111111111111111111111100,  // 0x0a
        0b00001111111111111111111111101001,  // 0x0b
        0b00001111111111111111111111101010,  // 0x0c
        0b00111111111111111111111111111101,  // 0x0d
        0b00001111111111111111111111101011,  // 0x0e
        0b00001111111111111111111111101100,  // 0x0f
        0b00001111111111111111111111101101,  // 0x10
        0b00001111111111111111111111101110,  // 0x11
        0b00001111111111111111111111101111,  // 0x12
        0b00001111111111111111111111110000,  // 0x13
        0b00001111111111111111111111110001,  // 0x14
        0b00001111111111111111111111110010,  // 0x15
        0b00111111111111111111111111111110,  // 0x16
        0b00001111111111111111111111110011,  // 0x17
        0b00001111111111111111111111110100,  // 0x18
        0b00001111111111111111111111110101,  // 0x19
        0b00001111111111111111111111110110,  // 0x1a
        0b00001111111111111111111111110111,  // 0x1b
        0b00001111111111111111111111111000,  // 0x1c
        0b00001111111111111111111111111001,  // 0x1d
        0b00001111111111111111111111111010,  // 0x1e
        0b00001111111111111111111111111011,  // 0x1f
        0b00000000000000000000000000010100,  // 0x20
        0b00000000000000000000001111111000,  // '!'
        0b00000000000000000000001111111001,  // '\"'
        0b00000000000000000000111111111010,  // '#'
        0b00000000000000000001111111111001,  // '$'
        0b00000000000000000000000000010101,  // '%'
        0b00000000000000000000000011111000,  // '&'
        0b00000000000000000000011111111010,  // '\''
        0b00000000000000000000001111111010,  // '('
        0b00000000000000000000001111111011,  // ')'
        0b00000000000000000000000011111001,  // '*'
        0b00000000000000000000011111111011,  // '+'
        0b00000000000000000000000011111010,  // ','
        0b00000000000000000000000000010110,  // '-'
        0b00000000000000000000000000010111,  // '.'
        0b00000000000000000000000000011000,  // '/'
        0b00000000000000000000000000000000,  // '0'
        0b00000000000000000000000000000001,  // '1'
        0b00000000000000000000000000000010,  // '2'
        0b00000000000000000000000000011001,  // '3'
        0b00000000000000000000000000011010,  // '4'
        0b00000000000000000000000000011011,  // '5'
        0b00000000000000000000000000011100,  // '6'
        0b00000000000000000000000000011101,  // '7'
        0b00000000000000000000000000011110,  // '8'
        0b00000000000000000000000000011111,  // '9'
        0b00000000000000000000000001011100,  // ':'
        0b00000000000000000000000011111011,  // ';'
        0b00000000000000000111111111111100,  // '<'
        0b00000000000000000000000000100000,  // '='
        0b00000000000000000000111111111011,  // '>'
        0b00000000000000000000001111111100,  // '?'
        0b00000000000000000001111111111010,  // '@'
        0b00000000000000000000000000100001,  // 'A'
        0b00000000000000000000000001011101,  // 'B'
        0b00000000000000000000000001011110,  // 'C'
        0b00000000000000000000000001011111,  // 'D'
        0b00000000000000000000000001100000,  // 'E'
        0b00000000000000000000000001100001,  // 'F'
        0b00000000000000000000000001100010,  // 'G'
        0b00000000000000000000000001100011,  // 'H'
        0b00000000000000000000000001100100,  // 'I'
        0b00000000000000000000000001100101,  // 'J'
        0b00000000000000000000000001100110,  // 'K'
        0b00000000000000000000000001100111,  // 'L'
        0b00000000000000000000000001101000,  // 'M'
        0b00000000000000000000000001101001,  // 'N'
        0b00000000000000000000000001101010,  // 'O'
        0b00000000000000000000000001101011,  // 'P'
        0b00000000000000000000000001101100,  // 'Q'
        0b00000000000000000000000001101101,  // 'R'
        0b00000000000000000000000001101110,  // 'S'
        0b00000000000000000000000001101111,  // 'T'
        0b00000000000000000000000001110000,  // 'U'
        0b00000000000000000000000001110001,  // 'V'
        0b00000000000000000000000001110010,  // 'W'
        0b00000000000000000000000011111100,  // 'X'
        0b00000000000000000000000001110011,  // 'Y'
        0b00000000000000000000000011111101,  // 'Z'
        0b00000000000000000001111111111011,  // '['
        0b00000000000001111111111111110000,  // '\\'
        0b00000000000000000001111111111100,  // ']'
        0b00000000000000000011111111111100,  // '^'
        0b00000000000000000000000000100010,  // '_'
        0b00000000000000000111111111111101,  // '`'
        0b00000000000000000000000000000011,  // 'a'
        0b00000000000000000000000000100011,  // 'b'
        0b00000000000000000000000000000100,  // 'c'
        0b00000000000000000000000000100100,  // 'd'
        0b00000000000000000000000000000101,  // 'e'
        0b00000000000000000000000000100101,  // 'f'
        0b00000000000000000000000000100110,  // 'g'
        0b00000000000000000000000000100111,  // 'h'
        0b00000000000000000000000000000110,  // 'i'
        0b00000000000000000000000001110100,  // 'j'
        0b00000000000000000000000001110101,  // 'k'
        0b00000000000000000000000000101000,  // 'l'
        0b00000000000000000000000000101001,  // 'm'
        0b00000000000000000000000000101010,  // 'n'
        0b00000000000000000000000000000111,  // 'o'
        0b00000000000000000000000000101011,  // 'p'
        0b00000000000000000000000001110110,  // 'q'
        0b00000000000000000000000000101100,  // 'r'
        0b00000000000000000000000000001000,  // 's'
        0b00000000000000000000000000001001,  // 't'
        0b00000000000000000000000000101101,  // 'u'
        0b00000000000000000000000001110111,  // 'v'
        0b00000000000000000000000001111000,  // 'w'
        0b00000000000000000000000001111001,  // 'x'
        0b00000000000000000000000001111010,  // 'y'
        0b00000000000000000000000001111011,  // 'z'
        0b00000000000000000111111111111110,  // '{'
        0b00000000000000000000011111111100,  // '|'
        0b00000000000000000011111111111101,  // '}'
        0b00000000000000000001111111111101,  // '~'
        0b00001111111111111111111111111100,  // 0x7f
        0b00000000000011111111111111100110,  // 0x80
        0b00000000001111111111111111010010,  // 0x81
        0b00000000000011111111111111100111,  // 0x82
        0b00000000000011111111111111101000,  // 0x83
        0b00000000001111111111111111010011,  // 0x84
        0b00000000001111111111111111010100,  // 0x85
        0b00000000001111111111111111010101,  // 0x86
        0b00000000011111111111111111011001,  // 0x87
        0b00000000001111111111111111010110,  // 0x88
        0b00000000011111111111111111011010,  // 0x89
        0b00000000011111111111111111011011,  // 0x8a
        0b00000000011111111111111111011100,  // 0x8b
        0b00000000011111111111111111011101,  // 0x8c
        0b00000000011111111111111111011110,  // 0x8d
        0b00000000111111111111111111101011,  // 0x8e
        0b00000000011111111111111111011111,  // 0x8f
        0b00000000111111111111111111101100,  // 0x90
        0b00000000111111111111111111101101,  // 0x91
        0b00000000001111111111111111010111,  // 0x92
        0b00000000011111111111111111100000,  // 0x93
        0b00000000111111111111111111101110,  // 0x94
        0b00000000011111111111111111100001,  // 0x95
        0b00000000011111111111111111100010,  // 0x96
        0b00000000011111111111111111100011,  // 0x97
        0b00000000011111111111111111100100,  // 0x98
        0b00000000000111111111111111011100,  // 0x99
        0b00000000001111111111111111011000,  // 0x9a
        0b00000000011111111111111111100101,  // 0x9b
        0b00000000001111111111111111011001,  // 0x9c
        0b00000000011111111111111111100110,  // 0x9d
        0b00000000011111111111111111100111,  // 0x9e
        0b00000000111111111111111111101111,  // 0x9f
        0b00000000001111111111111111011010,  // 0xa0
        0b00000000000111111111111111011101,  // 0xa1
        0b00000000000011111111111111101001,  // 0xa2
        0b00000000001111111111111111011011,  // 0xa3
        0b00000000001111111111111111011100,  // 0xa4
        0b00000000011111111111111111101000,  // 0xa5
        0b00000000011111111111111111101001,  // 0xa6
        0b00000000000111111111111111011110,  // 0xa7
        0b00000000011111111111111111101010,  // 0xa8
        0b00000000001111111111111111011101,  // 0xa9
        0b00000000001111111111111111011110,  // 0xaa
        0b00000000111111111111111111110000,  // 0xab
        0b00000000000111111111111111011111,  // 0xac
        0b00000000001111111111111111011111,  // 0xad
        0b00000000011111111111111111101011,  // 0xae
        0b00000000011111111111111111101100,  // 0xaf
        0b00000000000111111111111111100000,  // 0xb0
        0b00000000000111111111111111100001,  // 0xb1
        0b00000000001111111111111111100000,  // 0xb2
        0b00000000000111111111111111100010,  // 0xb3
        0b00000000011111111111111111101101,  // 0xb4
        0b00000000001111111111111111100001,  // 0xb5
        0b00000000011111111111111111101110,  // 0xb6
        0b00000000011111111111111111101111,  // 0xb7
        0b00000000000011111111111111101010,  // 0xb8
        0b00000000001111111111111111100010,  // 0xb9
        0b00000000001111111111111111100011,  // 0xba
        0b00000000001111111111111111100100,  // 0xbb
        0b00000000011111111111111111110000,  // 0xbc
        0b00000000001111111111111111100101,  // 0xbd
        0b00000000001111111111111111100110,  // 0xbe
        0b00000000011111111111111111110001,  // 0xbf
        0b00000011111111111111111111100000,  // 0xc0
        0b00000011111111111111111111100001,  // 0xc1
        0b00000000000011111111111111101011,  // 0xc2
        0b00000000000001111111111111110001,  // 0xc3
        0b00000000001111111111111111100111,  // 0xc4
        0b00000000011111111111111111110010,  // 0xc5
        0b00000000001111111111111111101000,  // 0xc6
        0b00000001111111111111111111101100,  // 0xc7
        0b00000011111111111111111111100010,  // 0xc8
        0b00000011111111111111111111100011,  // 0xc9
        0b00000011111111111111111111100100,  // 0xca
        0b00000111111111111111111111011110,  // 0xcb
        0b00000111111111111111111111011111,  // 0xcc
        0b00000011111111111111111111100101,  // 0xcd
        0b00000000111111111111111111110001,  // 0xce
        0b00000001111111111111111111101101,  // 0xcf
        0b00000000000001111111111111110010,  // 0xd0
        0b00000000000111111111111111100011,  // 0xd1
        0b00000011111111111111111111100110,  // 0xd2
        0b00000111111111111111111111100000,  // 0xd3
        0b00000111111111111111111111100001,  // 0xd4
        0b00000011111111111111111111100111,  // 0xd5
        0b00000111111111111111111111100010,  // 0xd6
        0b00000000111111111111111111110010,  // 0xd7
        0b00000000000111111111111111100100,  // 0xd8
        0b00000000000111111111111111100101,  // 0xd9
        0b00000011111111111111111111101000,  // 0xda
        0b00000011111111111111111111101001,  // 0xdb
        0b00001111111111111111111111111101,  // 0xdc
        0b00000111111111111111111111100011,  // 0xdd
        0b00000111111111111111111111100100,  // 0xde
        0b00000111111111111111111111100101,  // 0xdf
        0b00000000000011111111111111101100,  // 0xe0
        0b00000000111111111111111111110011,  // 0xe1
        0b00000000000011111111111111101101,  // 0xe2
        0b00000000000111111111111111100110,  // 0xe3
        0b00000000001111111111111111101001,  // 0xe4
        0b00000000000111111111111111100111,  // 0xe5
        0b00000000000111111111111111101000,  // 0xe6
        0b00000000011111111111111111110011,  // 0xe7
        0b00000000001111111111111111101010,  // 0xe8
        0b00000000001111111111111111101011,  // 0xe9
        0b00000001111111111111111111101110,  // 0xea
        0b00000001111111111111111111101111,  // 0xeb
        0b00000000111111111111111111110100,  // 0xec
        0b00000000111111111111111111110101,  // 0xed
        0b00000011111111111111111111101010,  // 0xee
        0b00000000011111111111111111110100,  // 0xef
        0b00000011111111111111111111101011,  // 0xf0
        0b00000111111111111111111111100110,  // 0xf1
        0b00000011111111111111111111101100,  // 0xf2
        0b00000011111111111111111111101101,  // 0xf3
        0b00000111111111111111111111100111,  // 0xf4
        0b00000111111111111111111111101000,  // 0xf5
        0b00000111111111111111111111101001,  // 0xf6
        0b00000111111111111111111111101010,  // 0xf7
        0b00000111111111111111111111101011,  // 0xf8
        0b00001111111111111111111111111110,  // 0xf9
        0b00000111111111111111111111101100,  // 0xfa
        0b00000111111111111111111111101101,  // 0xfb
        0b00000111111111111111111111101110,  // 0xfc
        0b00000111111111111111111111101111,  // 0xfd
        0b00000111111111111111111111110000,  // 0xfe
        0b00000011111111111111111111101110,  // 0xff
        0b00111111111111111111111111111111,  // 0x100
    };

    tree.push_back(HuffmanNode{});
    for (int symbol = 0; symbol <= 256; ++symbol) {
      uint8_t len = code_lengths[symbol];
      uint32_t code = right_codes[symbol];
      int curr = 0;
      for (int i = len - 1; i >= 0; --i) {
        int bit = (code >> i) & 1;
        if (tree[curr].child[bit] == -1) {
          tree[curr].child[bit] = tree.size();
          tree.push_back(HuffmanNode{});
        }
        curr = tree[curr].child[bit];
      }
      tree[curr].symbol = symbol;
    }
  });
  return tree;
}

inline bool DecodeHuffman(const uint8_t *buf, size_t len, std::string &out) {
  const auto &tree = GetHuffmanTree();
  int curr = 0;
  out.clear();
  for (size_t i = 0; i < len; ++i) {
    uint8_t byte = buf[i];
    for (int bit_idx = 7; bit_idx >= 0; --bit_idx) {
      int bit = (byte >> bit_idx) & 1;
      curr = tree[curr].child[bit];
      if (curr == -1) return false;
      if (tree[curr].symbol != -1) {
        if (tree[curr].symbol == 256) {
          return true;
        }
        out.push_back(static_cast<char>(tree[curr].symbol));
        curr = 0;
      }
    }
  }
  return true;
}

inline void EncodeInteger(std::vector<uint8_t> &buf, uint64_t value,
                          uint8_t prefix_mask) {
  uint8_t max_prefix_val = prefix_mask;
  if (value < max_prefix_val) {
    buf.back() |= static_cast<uint8_t>(value);
  } else {
    buf.back() |= max_prefix_val;
    value -= max_prefix_val;
    while (value >= 128) {
      buf.push_back(static_cast<uint8_t>((value % 128) + 128));
      value /= 128;
    }
    buf.push_back(static_cast<uint8_t>(value));
  }
}

inline bool DecodeInteger(const uint8_t *buf, size_t len, size_t &offset,
                          uint8_t prefix_mask, uint64_t &result) {
  if (offset >= len) return false;
  uint8_t first = buf[offset++];
  uint8_t val = first & prefix_mask;
  if (val < prefix_mask) {
    result = val;
    return true;
  }
  uint64_t m = 0;
  result = prefix_mask;
  while (offset < len) {
    uint8_t b = buf[offset++];
    result += static_cast<uint64_t>(b & 127) << m;
    m += 7;
    if ((b & 128) == 0) {
      return true;
    }
  }
  return false;
}

inline bool DecodeString(const uint8_t *buf, size_t len, size_t &offset,
                         std::string &out) {
  if (offset >= len) return false;
  uint8_t first = buf[offset];
  bool is_huffman = (first & 128) != 0;
  uint64_t str_len = 0;
  if (!DecodeInteger(buf, len, offset, 127, str_len)) return false;
  if (offset + str_len > len) return false;

  if (is_huffman) {
    if (!DecodeHuffman(buf + offset, str_len, out)) return false;
  } else {
    out.assign(reinterpret_cast<const char *>(buf + offset), str_len);
  }
  offset += str_len;
  return true;
}

inline void EncodeString(std::vector<uint8_t> &buf, const std::string &str) {
  buf.push_back(0);
  EncodeInteger(buf, str.size(), 127);
  buf.insert(buf.end(), str.begin(), str.end());
}

inline bool Decode(const uint8_t *buf, size_t len,
                   std::vector<std::pair<std::string, std::string>> &headers) {
  size_t offset = 0;
  uint64_t req_insert_count = 0;
  if (!DecodeInteger(buf, len, offset, 0xFF, req_insert_count)) return false;
  if (offset >= len) return false;
  uint64_t delta_base = 0;
  if (!DecodeInteger(buf, len, offset, 0x7F, delta_base)) return false;

  const auto &static_table = GetStaticTable();

  while (offset < len) {
    uint8_t type_byte = buf[offset];
    if (type_byte & 128) {
      bool is_static = (type_byte & 0x40) != 0;
      uint64_t index = 0;
      if (!DecodeInteger(buf, len, offset, 0x3F, index)) return false;
      if (!is_static) return false;
      if (index >= static_table.size()) return false;
      headers.push_back({static_table[index].name, static_table[index].value});
    } else if ((type_byte & 0xC0) == 0x40) {
      bool is_static = (type_byte & 0x10) != 0;
      uint64_t index = 0;
      if (!DecodeInteger(buf, len, offset, 0x0F, index)) return false;
      if (!is_static) return false;
      if (index >= static_table.size()) return false;
      std::string val;
      if (!DecodeString(buf, len, offset, val)) return false;
      headers.push_back({static_table[index].name, val});
    } else if ((type_byte & 0xE0) == 0x20) {
      bool is_huffman_name = (type_byte & 0x08) != 0;
      uint64_t name_len = 0;
      if (!DecodeInteger(buf, len, offset, 0x07, name_len)) return false;
      if (offset + name_len > len) return false;

      std::string name;
      if (is_huffman_name) {
        if (!DecodeHuffman(buf + offset, name_len, name)) return false;
      } else {
        name.assign(reinterpret_cast<const char *>(buf + offset), name_len);
      }
      offset += name_len;

      std::string val;
      if (!DecodeString(buf, len, offset, val)) return false;
      headers.push_back({name, val});
    } else {
      return false;
    }
  }
  return true;
}

inline std::vector<uint8_t> Encode(
    const std::vector<std::pair<std::string, std::string>> &headers) {
  std::vector<uint8_t> buf;
  buf.push_back(0x00);
  buf.push_back(0x00);

  const auto &static_table = GetStaticTable();

  for (const auto &p : headers) {
    int exact_idx = -1;
    int name_idx = -1;
    for (size_t i = 0; i < static_table.size(); ++i) {
      if (static_table[i].name == p.first) {
        if (name_idx == -1) name_idx = static_cast<int>(i);
        if (static_table[i].value == p.second) {
          exact_idx = static_cast<int>(i);
          break;
        }
      }
    }

    if (exact_idx != -1) {
      buf.push_back(0xC0);
      EncodeInteger(buf, exact_idx, 0x3F);
    } else if (name_idx != -1) {
      buf.push_back(0x50);
      EncodeInteger(buf, name_idx, 0x0F);
      EncodeString(buf, p.second);
    } else {
      buf.push_back(0x20);
      EncodeInteger(buf, p.first.size(), 0x07);
      buf.insert(buf.end(), p.first.begin(), p.first.end());
      EncodeString(buf, p.second);
    }
  }
  return buf;
}

}  // namespace qpack

namespace http3 {

inline bool DecodeVarint(const uint8_t *buf, size_t len, size_t &offset,
                         uint64_t &result) {
  if (offset >= len) return false;
  uint8_t first = buf[offset++];
  uint8_t type = (first >> 6) & 0x03;
  uint64_t val = first & 0x3F;
  int extra_bytes = 0;
  if (type == 0)
    extra_bytes = 0;
  else if (type == 1)
    extra_bytes = 1;
  else if (type == 2)
    extra_bytes = 3;
  else if (type == 3)
    extra_bytes = 7;

  if (offset + extra_bytes > len) return false;
  for (int i = 0; i < extra_bytes; ++i) {
    val = (val << 8) | buf[offset++];
  }
  result = val;
  return true;
}

inline void EncodeVarint(std::vector<uint8_t> &buf, uint64_t value) {
  if (value <= 63) {
    buf.push_back(static_cast<uint8_t>(value));
  } else if (value <= 16383) {
    buf.push_back(static_cast<uint8_t>((value >> 8) | 0x40));
    buf.push_back(static_cast<uint8_t>(value & 0xFF));
  } else if (value <= 1073741823) {
    buf.push_back(static_cast<uint8_t>((value >> 24) | 0x80));
    buf.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(value & 0xFF));
  } else {
    buf.push_back(static_cast<uint8_t>((value >> 56) | 0xC0));
    buf.push_back(static_cast<uint8_t>((value >> 48) & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 40) & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 32) & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(value & 0xFF));
  }
}

inline std::vector<uint8_t> EncodeFrame(uint64_t type,
                                        const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> buf;
  EncodeVarint(buf, type);
  EncodeVarint(buf, payload.size());
  buf.insert(buf.end(), payload.begin(), payload.end());
  return buf;
}

inline std::vector<uint8_t> GetSettingsPayload() {
  std::vector<uint8_t> payload;
  EncodeVarint(payload, 0x01);
  EncodeVarint(payload, 0);
  EncodeVarint(payload, 0x06);
  EncodeVarint(payload, 1048576);
  return payload;
}

inline std::string ReconstructHttp11Request(
    const std::vector<std::pair<std::string, std::string>> &headers,
    const std::vector<uint8_t> &body) {
  std::string method = "GET";
  std::string path = "/";
  std::string host = "";
  std::string req;

  for (const auto &p : headers) {
    if (p.first == ":method")
      method = p.second;
    else if (p.first == ":path")
      path = p.second;
    else if (p.first == ":authority")
      host = p.second;
  }

  req += method + " " + path + " HTTP/1.1\r\n";
  if (!host.empty()) {
    req += "Host: " + host + "\r\n";
  }

  for (const auto &p : headers) {
    if (p.first[0] == ':') continue;
    if (p.first == "host" && !host.empty()) continue;
    req += p.first + ": " + p.second + "\r\n";
  }

  req += "\r\n";
  req.insert(req.end(), body.begin(), body.end());
  return req;
}

inline std::string ReconstructHttp11Response(
    const std::vector<std::pair<std::string, std::string>> &headers,
    const std::vector<uint8_t> &body) {
  std::string status = "200";
  std::string status_text = "OK";
  std::string resp;

  for (const auto &p : headers) {
    if (p.first == ":status") {
      status = p.second;
    }
  }

  resp += "HTTP/1.1 " + status + " " + status_text + "\r\n";

  bool has_cl = false;
  for (const auto &p : headers) {
    if (p.first[0] == ':') continue;
    if (p.first == "content-length") has_cl = true;
    resp += p.first + ": " + p.second + "\r\n";
  }
  if (!has_cl) {
    resp += "Content-Length: " + std::to_string(body.size()) + "\r\n";
  }

  resp += "\r\n";
  resp.insert(resp.end(), body.begin(), body.end());
  return resp;
}

inline bool ParseHttp11Response(
    const std::string &data,
    std::vector<std::pair<std::string, std::string>> &headers,
    std::vector<uint8_t> &body) {
  size_t pos = 0;
  size_t line_end = data.find("\r\n", pos);
  if (line_end == std::string::npos) return false;
  std::string status_line = data.substr(pos, line_end - pos);
  pos = line_end + 2;

  size_t first_space = status_line.find(' ');
  if (first_space != std::string::npos) {
    size_t second_space = status_line.find(' ', first_space + 1);
    std::string status_code =
        status_line.substr(first_space + 1, second_space - first_space - 1);
    headers.push_back({":status", status_code});
  } else {
    headers.push_back({":status", "200"});
  }

  while (pos < data.size()) {
    size_t next_line = data.find("\r\n", pos);
    if (next_line == std::string::npos) break;
    if (next_line == pos) {
      pos += 2;
      break;
    }
    std::string line = data.substr(pos, next_line - pos);
    pos = next_line + 2;

    size_t colon = line.find(':');
    if (colon != std::string::npos) {
      std::string name = line.substr(0, colon);
      std::string val = line.substr(colon + 1);
      while (!name.empty() && name.back() == ' ') name.pop_back();
      while (!name.empty() && name.front() == ' ') name.erase(name.begin());
      while (!val.empty() && val.back() == ' ') val.pop_back();
      while (!val.empty() && val.front() == ' ') val.erase(val.begin());
      for (auto &c : name) c = std::tolower(c);
      headers.push_back({name, val});
    }
  }

  if (pos < data.size()) {
    body.assign(data.begin() + pos, data.end());
  }
  return true;
}

}  // namespace http3

// ============================================================================
// Transport Abstractions (TCP & QUIC)
// ============================================================================

// TCP server transport wrapper
class TcpServerTransport {
 public:
  struct ConnectionEvent {
    uint64_t session_id;
    cpptcpnet::ConnectionState state;
  };
  struct TransferEvent {
    uint64_t session_id;
    bool is_send;
    size_t bytes_transferred;
  };

  TcpServerTransport(uint16_t port, const std::string &bind_address = "0.0.0.0")
      : port_(port),
        listener_(
            std::make_shared<cpptcpnet::TcpListener>(port, bind_address)) {
    state_sub_ =
        listener_->GetEventBroker().Subscribe<cpptcpnet::ConnectionEvent>(
            "state_events");
    transfer_sub_ =
        listener_->GetEventBroker().Subscribe<cpptcpnet::TransferEvent>(
            "transfer_events");
  }

  void Start() { listener_->Start(); }

  void Stop() { listener_->Stop(); }

  uint16_t GetLocalPort() const { return port_; }

  bool Send(uint64_t session_id, const std::string &data) {
    return listener_->Send(session_id, data);
  }

  bool Send(uint64_t session_id, const std::vector<uint8_t> &data) {
    return listener_->Send(session_id, data);
  }

  void Disconnect(uint64_t session_id) { listener_->Disconnect(session_id); }

  void SetSendChunkSize(size_t size) { listener_->SetSendChunkSize(size); }
  void SetMaxOutboundBufferSize(size_t size) {
    listener_->SetMaxOutboundBufferSize(size);
  }

#ifdef CPPHTTP_ENABLE_QUIC
  void SetQuicProfile(const cppquic::QuicProfile &profile) {}
#endif

  cpptcpnet::ConnectionProfile GetConnectionProfile(uint64_t session_id) const {
    return listener_->GetConnectionProfile(session_id);
  }

  void ApplyConnectionProfile(uint64_t session_id,
                              const cpptcpnet::ConnectionProfile &profile) {
    listener_->ApplyConnectionProfile(session_id, profile);
  }

  cpptcpnet::PeerAddress GetPeerAddress(uint64_t session_id) const {
    return listener_->GetPeerAddress(session_id);
  }

  std::optional<TransferEvent> TryReceiveTransferEvent() {
    if (auto ev = transfer_sub_->try_receive()) {
      return TransferEvent{ev->session_id, ev->is_send, ev->bytes_transferred};
    }
    return std::nullopt;
  }

  std::optional<ConnectionEvent> TryReceiveConnectionEvent() {
    if (auto ev = state_sub_->try_receive()) {
      return ConnectionEvent{ev->session_id, ev->state};
    }
    return std::nullopt;
  }

  void SetDataHandler(
      std::function<void(uint64_t session_id, const std::vector<uint8_t> &data)>
          handler) {
    listener_->SetDataHandler(std::move(handler));
  }

  bool SupportsQuic() const { return false; }
  bool IsQuicSession(uint64_t session_id) const { return false; }

  cpptcpnet::TcpListener &GetListener() { return *listener_; }
  const cpptcpnet::TcpListener &GetListener() const { return *listener_; }

 private:
  uint16_t port_;
  std::shared_ptr<cpptcpnet::TcpListener> listener_;
  std::shared_ptr<cpppubsub::Subscriber<cpptcpnet::ConnectionEvent>> state_sub_;
  std::shared_ptr<cpppubsub::Subscriber<cpptcpnet::TransferEvent>>
      transfer_sub_;
};

// TCP client transport wrapper
class TcpClientTransport {
 public:
  TcpClientTransport() : session_id_(0) {}

  ~TcpClientTransport() { Stop(); }

  void EnableSSL() { ssl_enabled_ = true; }
  void Start() {}
  void Stop() {
    std::unique_ptr<cpptcpnet::TcpClient> old_client;
    std::unique_ptr<cpppubsub::Worker> old_worker;
    {
      std::lock_guard<std::mutex> lock(mtx_);
      old_client = std::move(client_);
      old_worker = std::move(worker_);
      state_sub_.reset();
      session_id_ = 0;
    }
    if (old_client || old_worker) {
      std::thread([c = std::move(old_client), w = std::move(old_worker)]() {
        if (c) c->Stop();
        if (w) w->Stop();
      }).detach();
    }
  }

  uint64_t Connect(const std::string &host, uint16_t port,
                   std::chrono::milliseconds timeout) {
    Stop();

    auto client = std::make_unique<cpptcpnet::TcpClient>();
    if (ssl_enabled_) {
      client->EnableSSL();
    }
    client->SetDataHandler(
        [this](uint64_t session_id, const std::vector<uint8_t> &data) {
          std::lock_guard<std::mutex> lock(mtx_);
          if (session_id != session_id_) return;
          if (data_handler_) {
            data_handler_(session_id, data);
          }
        });

    auto state_sub =
        client->GetEventBroker().Subscribe<cpptcpnet::ConnectionEvent>(
            "state_events");

    auto connected = std::make_shared<std::atomic<bool>>(false);
    auto disconnected = std::make_shared<std::atomic<bool>>(false);
    auto connect_mtx = std::make_shared<std::mutex>();
    auto connect_cv = std::make_shared<std::condition_variable>();

    auto worker = std::make_unique<cpppubsub::Worker>();
    worker->AddSubscription<cpptcpnet::ConnectionEvent>(
        state_sub, [this, connected, disconnected, connect_mtx,
                    connect_cv](const cpptcpnet::ConnectionEvent &e) {
          {
            std::lock_guard<std::mutex> lock(*connect_mtx);
            if (e.session_id == 0) return;
            if (e.state == cpptcpnet::ConnectionState::Disconnected) {
              *disconnected = true;
              connect_cv->notify_all();
            } else if (e.state == cpptcpnet::ConnectionState::Connected) {
              *connected = true;
              connect_cv->notify_all();
            }
          }
          if (e.state == cpptcpnet::ConnectionState::Disconnected) {
            std::lock_guard<std::mutex> lock(mtx_);
            if (disconnect_handler_) {
              disconnect_handler_();
            }
          }
        });
    worker->Start();
    client->Start();

    uint64_t sid = client->Connect(host, port);
    if (sid == 0) {
      client->Stop();
      worker->Stop();
      return 0;
    }

    {
      std::lock_guard<std::mutex> lock(mtx_);
      client_ = std::move(client);
      worker_ = std::move(worker);
      state_sub_ = state_sub;
      session_id_ = sid;
    }

    std::unique_lock<std::mutex> lock(*connect_mtx);
    if (!connect_cv->wait_for(lock, timeout, [&] {
          return connected->load() || disconnected->load();
        })) {
      lock.unlock();
      Stop();
      return 0;
    }
    if (disconnected->load() || !connected->load()) {
      lock.unlock();
      Stop();
      return 0;
    }

    return sid;
  }

  bool Send(uint64_t session_id, const std::string &data) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!client_ || session_id != session_id_) return false;
    return client_->Send(session_id, data);
  }

  bool Send(uint64_t session_id, const std::vector<uint8_t> &data) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!client_ || session_id != session_id_) return false;
    return client_->Send(session_id, data);
  }

  void SetDataHandler(
      std::function<void(uint64_t session_id, const std::vector<uint8_t> &data)>
          handler) {
    std::lock_guard<std::mutex> lock(mtx_);
    data_handler_ = std::move(handler);
  }

  void SetDisconnectHandler(std::function<void()> handler) {
    std::lock_guard<std::mutex> lock(mtx_);
    disconnect_handler_ = std::move(handler);
  }

  void StartRequest(uint64_t session_id) {}
  void EndRequest(uint64_t session_id) {}

  void SetSendChunkSize(size_t size) {}
  void SetMaxOutboundBufferSize(size_t size) {}

#ifdef CPPHTTP_ENABLE_QUIC
  void SetQuicProfile(const cppquic::QuicProfile &profile) {}
#endif

  cpptcpnet::ConnectionProfile GetConnectionProfile(uint64_t session_id) const {
    std::lock_guard<std::mutex> lock(*const_cast<std::mutex *>(&mtx_));
    if (!client_) {
      throw std::runtime_error("Client not connected");
    }
    return client_->GetConnectionProfile(session_id);
  }

  std::shared_ptr<cpppubsub::Subscriber<cpptcpnet::TransferEvent>>
  GetTransferSubscriber() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!client_) {
      return std::shared_ptr<cpppubsub::Subscriber<cpptcpnet::TransferEvent>>();
    }
    return client_->GetEventBroker().Subscribe<cpptcpnet::TransferEvent>(
        "transfer_events");
  }

 private:
  std::unique_ptr<cpptcpnet::TcpClient> client_;
  std::unique_ptr<cpppubsub::Worker> worker_;
  std::shared_ptr<cpppubsub::Subscriber<cpptcpnet::ConnectionEvent>> state_sub_;
  uint64_t session_id_ = 0;
  bool ssl_enabled_ = false;
  std::function<void(uint64_t, const std::vector<uint8_t> &)> data_handler_;
  std::function<void()> disconnect_handler_;
  std::mutex mtx_;
};

#ifdef CPPHTTP_ENABLE_QUIC
// QUIC server transport wrapper
class QuicServerTransport {
 public:
  struct ConnectionEvent {
    uint64_t session_id;
    cpptcpnet::ConnectionState state;
  };
  struct TransferEvent {
    uint64_t session_id;
    bool is_send;
    size_t bytes_transferred;
  };

  QuicServerTransport(uint16_t port,
                      const std::string &bind_address = "0.0.0.0")
      : server_(port, bind_address) {
    server_.SetStreamDataHandler(
        [this](std::shared_ptr<cppquic::QuicConnection> conn,
               uint64_t stream_id, const std::vector<uint8_t> &data,
               bool fin) { HandleQuicStreamData(conn, stream_id, data, fin); });
  }

  void Start() {
    conn_sub_ = server_.GetEventBroker().Subscribe<cppquic::ConnectionEvent>(
        "connection_events");
    server_.Start();
  }

  void Stop() {
    server_.Stop();
    std::lock_guard<std::mutex> lock(mapping_mutex_);
    key_to_id_.clear();
    id_to_key_.clear();
    id_to_conn_.clear();
  }

  uint16_t GetLocalPort() const { return server_.GetLocalPort(); }

  bool Send(uint64_t session_id, const std::string &data) {
    std::shared_ptr<cppquic::QuicConnection> conn;
    uint64_t stream_id = 0;
    bool is_h3 = false;
    {
      std::lock_guard<std::mutex> lock(mapping_mutex_);
      auto it = id_to_key_.find(session_id);
      if (it == id_to_key_.end()) return false;
      conn = id_to_conn_[session_id];
      stream_id = it->second.stream_id;

      auto conn_id = conn->GetLocalConnectionId();
      auto conn_it = h3_connections_.find(conn_id);
      if (conn_it != h3_connections_.end()) {
        is_h3 = conn_it->second.is_http3;
      }
    }
    if (!conn) return false;

    if (is_h3) {
      std::vector<std::pair<std::string, std::string>> headers;
      std::vector<uint8_t> body;
      if (http3::ParseHttp11Response(data, headers, body)) {
        // Strip connection-specific headers (RFC 9114, Section 4.1.2)
        std::vector<std::pair<std::string, std::string>> clean_headers;
        clean_headers.reserve(headers.size());
        for (const auto &pair : headers) {
          std::string lower_key = pair.first;
          std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(),
                         [](unsigned char c) { return std::tolower(c); });
          if (lower_key == "connection" || lower_key == "keep-alive" ||
              lower_key == "proxy-connection" ||
              lower_key == "transfer-encoding" || lower_key == "upgrade") {
            continue;
          }
          clean_headers.push_back(pair);
        }
        std::vector<uint8_t> qpack_block = qpack::Encode(clean_headers);
        std::vector<uint8_t> headers_frame =
            http3::EncodeFrame(0x01, qpack_block);
        std::vector<uint8_t> data_to_send = headers_frame;
        if (!body.empty()) {
          std::vector<uint8_t> data_frame = http3::EncodeFrame(0x00, body);
          data_to_send.insert(data_to_send.end(), data_frame.begin(),
                              data_frame.end());
        }
        server_.SendOnStream(conn, stream_id, data_to_send, true);
      } else {
        server_.SendOnStream(conn, stream_id, data);
      }
    } else {
      server_.SendOnStream(conn, stream_id, data, true);
    }

    {
      std::lock_guard<std::mutex> lock(events_mutex_);
      transfer_events_.push_back({session_id, true, data.size()});
    }
    return true;
  }

  bool Send(uint64_t session_id, const std::vector<uint8_t> &data) {
    std::string str(data.begin(), data.end());
    return Send(session_id, str);
  }

  void Disconnect(uint64_t session_id) {
    std::shared_ptr<cppquic::QuicConnection> conn;
    uint64_t stream_id = 0;
    {
      std::lock_guard<std::mutex> lock(mapping_mutex_);
      auto it = id_to_key_.find(session_id);
      if (it != id_to_key_.end()) {
        conn = id_to_conn_[session_id];
        stream_id = it->second.stream_id;
        key_to_id_.erase(it->second);
        id_to_conn_.erase(session_id);
        id_to_key_.erase(it);
      }
    }
    if (conn) {
      server_.SendOnStream(conn, stream_id, std::vector<uint8_t>{}, true);
    }
  }

  void SetSendChunkSize(size_t size) { send_chunk_size_ = size; }
  void SetMaxOutboundBufferSize(size_t size) {
    max_outbound_buffer_size_ = size;
  }

  cpptcpnet::ConnectionProfile GetConnectionProfile(uint64_t session_id) const {
    cpptcpnet::ConnectionProfile profile;
    profile.max_outbound_buffer_size = max_outbound_buffer_size_;
    profile.send_chunk_size = send_chunk_size_;
    return profile;
  }

  void ApplyConnectionProfile(uint64_t session_id,
                              const cpptcpnet::ConnectionProfile &profile) {}

  cpptcpnet::PeerAddress GetPeerAddress(uint64_t session_id) const {
    std::shared_ptr<cppquic::QuicConnection> conn;
    {
      std::lock_guard<std::mutex> lock(mapping_mutex_);
      auto it = id_to_conn_.find(session_id);
      if (it != id_to_conn_.end()) {
        conn = it->second;
      }
    }
    cpptcpnet::PeerAddress addr;
    if (conn) {
      auto peer = conn->GetPeer();
      addr.ip = peer.ip;
      addr.port = peer.port;
    }
    return addr;
  }

  std::optional<TransferEvent> TryReceiveTransferEvent() {
    std::lock_guard<std::mutex> lock(events_mutex_);
    if (!transfer_events_.empty()) {
      auto ev = transfer_events_.front();
      transfer_events_.pop_front();
      return ev;
    }
    return std::nullopt;
  }

  std::optional<ConnectionEvent> TryReceiveConnectionEvent() {
    if (conn_sub_) {
      while (auto ev = conn_sub_->try_receive()) {
        if (ev->state == cppquic::ConnectionState::Closed) {
          std::vector<uint64_t> sessions_to_disconnect;
          {
            std::lock_guard<std::mutex> lock(mapping_mutex_);
            h3_connections_.erase(ev->connection_id);
            for (auto it = id_to_key_.begin(); it != id_to_key_.end();) {
              if (it->second.conn_id == ev->connection_id) {
                sessions_to_disconnect.push_back(it->first);
                key_to_id_.erase(it->second);
                id_to_conn_.erase(it->first);
                it = id_to_key_.erase(it);
              } else {
                ++it;
              }
            }
          }
          std::lock_guard<std::mutex> lock(events_mutex_);
          for (uint64_t session_id : sessions_to_disconnect) {
            connection_events_.push_back(
                {session_id, cpptcpnet::ConnectionState::Disconnected});
          }
        }
      }
    }

    std::lock_guard<std::mutex> lock(events_mutex_);
    if (!connection_events_.empty()) {
      auto ev = connection_events_.front();
      connection_events_.pop_front();
      return ev;
    }
    return std::nullopt;
  }

  void SetDataHandler(
      std::function<void(uint64_t session_id, const std::vector<uint8_t> &data)>
          handler) {
    data_handler_ = std::move(handler);
  }

  bool SupportsQuic() const { return true; }
  bool IsQuicSession(uint64_t session_id) const { return true; }

  cppquic::QuicServer &GetServer() { return server_; }
  const cppquic::QuicServer &GetServer() const { return server_; }

  void SetQuicProfile(const cppquic::QuicProfile &profile) {
    server_.SetQuicProfile(profile);
    if (profile.initial_max_data >= 32 * 1024 * 1024) {
      send_chunk_size_ = 128 * 1024;
    }
  }

 private:
  size_t send_chunk_size_ = 65536;
  size_t max_outbound_buffer_size_ = 1024 * 1024;

 private:
  struct SessionKey {
    cppquic::ConnectionId conn_id;
    uint64_t stream_id;
    bool operator==(const SessionKey &o) const {
      return conn_id == o.conn_id && stream_id == o.stream_id;
    }
  };
  struct SessionKeyHash {
    size_t operator()(const SessionKey &k) const {
      return cppquic::ConnectionIdHash{}(k.conn_id) ^
             std::hash<uint64_t>{}(k.stream_id);
    }
  };

  void HandleQuicStreamData(std::shared_ptr<cppquic::QuicConnection> conn,
                            uint64_t stream_id,
                            const std::vector<uint8_t> &data, bool fin) {
    uint64_t session_id = 0;
    bool is_new = false;
    bool is_h3_session = false;

    {
      std::lock_guard<std::mutex> lock(mapping_mutex_);
      auto conn_id = conn->GetLocalConnectionId();
      auto &conn_state = h3_connections_[conn_id];
      conn_state.conn = conn;

      if (!conn_state.is_http3 && !data.empty()) {
        if (data[0] < 0x20) {
          conn_state.is_http3 = true;

          if (!conn_state.server_ctrl_opened) {
            uint64_t ctrl_id = conn->AllocateStreamId(false);
            std::vector<uint8_t> ctrl_payload = {0x00};
            std::vector<uint8_t> settings_frame =
                http3::EncodeFrame(0x04, http3::GetSettingsPayload());
            ctrl_payload.insert(ctrl_payload.end(), settings_frame.begin(),
                                settings_frame.end());
            server_.SendOnStream(conn, ctrl_id, ctrl_payload);

            uint64_t qpack_enc_id = conn->AllocateStreamId(false);
            server_.SendOnStream(conn, qpack_enc_id,
                                 std::vector<uint8_t>{0x02});

            uint64_t qpack_dec_id = conn->AllocateStreamId(false);
            server_.SendOnStream(conn, qpack_dec_id,
                                 std::vector<uint8_t>{0x03});

            conn_state.server_ctrl_opened = true;
          }
        }
      }

      is_h3_session = conn_state.is_http3;
    }

    if (is_h3_session) {
      std::vector<uint8_t> h3_to_http11_data;
      bool h3_request_complete = false;
      bool violated = false;

      {
        std::lock_guard<std::mutex> lock(mapping_mutex_);
        auto conn_id = conn->GetLocalConnectionId();
        auto &conn_state = h3_connections_[conn_id];

        if (conn_state.protocol_violated) {
          violated = true;
        } else {
          auto &stream_state = conn_state.streams[stream_id];
          stream_state.stream_id = stream_id;
          stream_state.recv_buf.insert(stream_state.recv_buf.end(),
                                       data.begin(), data.end());

          if ((stream_id & 0x02) != 0) {
            if (!stream_state.is_control && !stream_state.is_qpack_enc &&
                !stream_state.is_qpack_dec && !stream_state.recv_buf.empty()) {
              uint8_t type = stream_state.recv_buf.front();
              stream_state.recv_buf.erase(stream_state.recv_buf.begin());
              if (type == 0x00) {
                if (conn_state.client_ctrl_stream_opened) {
                  conn_state.protocol_violated = true;
                } else {
                  conn_state.client_ctrl_stream_opened = true;
                  stream_state.is_control = true;
                  conn_state.client_ctrl_stream_id = stream_id;
                }
              } else if (type == 0x02) {
                if (conn_state.client_qpack_enc_opened) {
                  conn_state.protocol_violated = true;
                } else {
                  conn_state.client_qpack_enc_opened = true;
                  stream_state.is_qpack_enc = true;
                }
              } else if (type == 0x03) {
                if (conn_state.client_qpack_dec_opened) {
                  conn_state.protocol_violated = true;
                } else {
                  conn_state.client_qpack_dec_opened = true;
                  stream_state.is_qpack_dec = true;
                }
              }
            }

            if (conn_state.protocol_violated) {
              violated = true;
            } else if (stream_state.is_control) {
              size_t offset = 0;
              while (offset < stream_state.recv_buf.size()) {
                size_t frame_start = offset;
                uint64_t type = 0, length = 0;
                if (http3::DecodeVarint(stream_state.recv_buf.data(),
                                        stream_state.recv_buf.size(), offset,
                                        type) &&
                    http3::DecodeVarint(stream_state.recv_buf.data(),
                                        stream_state.recv_buf.size(), offset,
                                        length)) {
                  if (!conn_state.client_settings_received) {
                    if (type != 0x04) {
                      conn_state.protocol_violated = true;
                      violated = true;
                      break;
                    }
                    conn_state.client_settings_received = true;
                  }
                  if (type == 0x00 || type == 0x01) {
                    conn_state.protocol_violated = true;
                    violated = true;
                    break;
                  }
                  if (offset + length <= stream_state.recv_buf.size()) {
                    offset += length;
                  } else {
                    offset = frame_start;
                    break;
                  }
                } else {
                  offset = frame_start;
                  break;
                }
              }
              if (offset > 0) {
                stream_state.recv_buf.erase(
                    stream_state.recv_buf.begin(),
                    stream_state.recv_buf.begin() + offset);
              }
            } else {
              stream_state.recv_buf.clear();
            }
          } else {
            size_t offset = 0;
            while (offset < stream_state.recv_buf.size()) {
              size_t frame_start = offset;
              uint64_t type = 0, length = 0;
              if (http3::DecodeVarint(stream_state.recv_buf.data(),
                                      stream_state.recv_buf.size(), offset,
                                      type) &&
                  http3::DecodeVarint(stream_state.recv_buf.data(),
                                      stream_state.recv_buf.size(), offset,
                                      length)) {
                if (type == 0x04) {
                  conn_state.protocol_violated = true;
                  violated = true;
                  break;
                }
                if (offset + length <= stream_state.recv_buf.size()) {
                  const uint8_t *payload =
                      stream_state.recv_buf.data() + offset;
                  if (type == 0x01) {
                    qpack::Decode(payload, length, stream_state.headers);
                  } else if (type == 0x00) {
                    stream_state.body.insert(stream_state.body.end(), payload,
                                             payload + length);
                  }
                  offset += length;
                } else {
                  offset = frame_start;
                  break;
                }
              } else {
                offset = frame_start;
                break;
              }
            }
            if (offset > 0) {
              stream_state.recv_buf.erase(
                  stream_state.recv_buf.begin(),
                  stream_state.recv_buf.begin() + offset);
            }

            if (fin && !conn_state.protocol_violated) {
              std::string req_str = http3::ReconstructHttp11Request(
                  stream_state.headers, stream_state.body);
              h3_to_http11_data.assign(req_str.begin(), req_str.end());
              h3_request_complete = true;
            }
          }
        }
      }

      if (violated) {
        internal::Log(
            LogSeverity::Warning, "QuicServer",
            "HTTP/3 protocol violation detected. Closing connection.");
        conn->SetState(cppquic::ConnectionState::Closed);
        return;
      }

      if (h3_request_complete) {
        {
          std::lock_guard<std::mutex> lock(mapping_mutex_);
          SessionKey key{conn->GetLocalConnectionId(), stream_id};
          auto it = key_to_id_.find(key);
          if (it == key_to_id_.end()) {
            session_id = next_session_id_++;
            key_to_id_[key] = session_id;
            id_to_key_[session_id] = key;
            id_to_conn_[session_id] = conn;
            is_new = true;
          } else {
            session_id = it->second;
          }
        }

        if (is_new) {
          std::lock_guard<std::mutex> lock(events_mutex_);
          connection_events_.push_back(
              {session_id, cpptcpnet::ConnectionState::Connected});
        }

        if (data_handler_ && !h3_to_http11_data.empty()) {
          data_handler_(session_id, h3_to_http11_data);
        }
      }

      return;
    }

    {
      std::lock_guard<std::mutex> lock(mapping_mutex_);
      SessionKey key{conn->GetLocalConnectionId(), stream_id};
      auto it = key_to_id_.find(key);
      if (it == key_to_id_.end()) {
        session_id = next_session_id_++;
        key_to_id_[key] = session_id;
        id_to_key_[session_id] = key;
        id_to_conn_[session_id] = conn;
        is_new = true;
      } else {
        session_id = it->second;
      }
    }

    if (is_new) {
      std::lock_guard<std::mutex> lock(events_mutex_);
      connection_events_.push_back(
          {session_id, cpptcpnet::ConnectionState::Connected});
    }

    if (data_handler_ && !data.empty()) {
      data_handler_(session_id, data);
    }

    if (fin) {
      std::lock_guard<std::mutex> lock(events_mutex_);
      connection_events_.push_back(
          {session_id, cpptcpnet::ConnectionState::Disconnected});
    }
  }

  struct Http3StreamState {
    uint64_t stream_id;
    std::vector<uint8_t> recv_buf;
    bool is_control = false;
    bool is_qpack_enc = false;
    bool is_qpack_dec = false;
    std::vector<std::pair<std::string, std::string>> headers;
    std::vector<uint8_t> body;
  };
  struct Http3ConnState {
    std::shared_ptr<cppquic::QuicConnection> conn;
    bool is_http3 = false;
    bool server_ctrl_opened = false;
    uint64_t client_ctrl_stream_id = 0xFFFFFFFFFFFFFFFFULL;
    std::unordered_map<uint64_t, Http3StreamState> streams;
    bool client_ctrl_stream_opened = false;
    bool client_qpack_enc_opened = false;
    bool client_qpack_dec_opened = false;
    bool client_settings_received = false;
    bool protocol_violated = false;
  };
  std::unordered_map<cppquic::ConnectionId, Http3ConnState,
                     cppquic::ConnectionIdHash>
      h3_connections_;

  cppquic::QuicServer server_;
  std::shared_ptr<cpppubsub::Subscriber<cppquic::ConnectionEvent>> conn_sub_;
  std::function<void(uint64_t session_id, const std::vector<uint8_t> &data)>
      data_handler_;

  mutable std::mutex mapping_mutex_;
  std::unordered_map<SessionKey, uint64_t, SessionKeyHash> key_to_id_;
  std::unordered_map<uint64_t, SessionKey> id_to_key_;
  mutable std::unordered_map<uint64_t, std::shared_ptr<cppquic::QuicConnection>>
      id_to_conn_;
  uint64_t next_session_id_ = 1;

  mutable std::mutex events_mutex_;
  std::deque<ConnectionEvent> connection_events_;
  std::deque<TransferEvent> transfer_events_;
};

// QUIC client transport wrapper
class QuicClientTransport {
 public:
  QuicClientTransport() {
    client_.SetStreamDataHandler(
        [this](uint64_t stream_id, const std::vector<uint8_t> &data, bool fin) {
          if (stream_id != active_stream_id_) return;

          recv_buf_.insert(recv_buf_.end(), data.begin(), data.end());
          size_t offset = 0;
          while (offset < recv_buf_.size()) {
            size_t frame_start = offset;
            uint64_t type = 0, length = 0;
            if (http3::DecodeVarint(recv_buf_.data(), recv_buf_.size(), offset,
                                    type) &&
                http3::DecodeVarint(recv_buf_.data(), recv_buf_.size(), offset,
                                    length)) {
              if (type == 0x00 || type == 0x01) {
                is_h3_ = true;
                if (offset + length <= recv_buf_.size()) {
                  const uint8_t *payload = recv_buf_.data() + offset;
                  if (type == 0x01) {
                    qpack::Decode(payload, length, resp_headers_);
                    std::string status = "200";
                    for (const auto &p : resp_headers_) {
                      if (p.first == ":status") status = p.second;
                    }
                    std::string header_str = "HTTP/1.1 " + status + " OK\r\n";
                    for (const auto &p : resp_headers_) {
                      if (p.first[0] == ':') continue;
                      header_str += p.first + ": " + p.second + "\r\n";
                    }
                    header_str += "\r\n";
                    std::vector<uint8_t> h_vec(header_str.begin(),
                                               header_str.end());
                    if (data_handler_) data_handler_(1, h_vec);
                    headers_sent_ = true;
                  } else if (type == 0x00) {
                    std::vector<uint8_t> d_vec(payload, payload + length);
                    if (data_handler_) data_handler_(1, d_vec);
                  }
                  offset += length;
                } else {
                  offset = frame_start;
                  break;
                }
              } else {
                offset = frame_start;
                break;
              }
            } else {
              offset = frame_start;
              break;
            }
          }
          if (offset > 0) {
            recv_buf_.erase(recv_buf_.begin(), recv_buf_.begin() + offset);
          }

          if (!is_h3_) {
            if (data_handler_ && !data.empty()) {
              data_handler_(1, data);
            }
            recv_buf_.clear();
          }

          if (fin) {
            stream_finished_ = true;
            is_h3_ = false;
            headers_sent_ = false;
            resp_headers_.clear();
            recv_buf_.clear();
            if (disconnect_handler_) {
              disconnect_handler_();
            }
          }
        });

    client_.SetErrorHandler([this](int code, const std::string &msg) {
      std::lock_guard<std::mutex> lock(mtx_);
      if (disconnect_handler_) {
        disconnect_handler_();
      }
    });
  }

  void EnableSSL() {}
  void Start() {}
  void Stop() { client_.Stop(); }

  uint64_t Connect(const std::string &host, uint16_t port,
                   std::chrono::milliseconds timeout) {
    client_.Start();
    bool ok = client_.Connect(host, port, timeout.count());
    if (ok) {
      return 1;
    }
    return 0;
  }

  bool Send(uint64_t session_id, const std::string &data) {
    std::lock_guard<std::mutex> lock(mtx_);
    client_.SendOnStream(active_stream_id_, data, true);
    return true;
  }

  bool Send(uint64_t session_id, const std::vector<uint8_t> &data) {
    std::lock_guard<std::mutex> lock(mtx_);
    client_.SendOnStream(active_stream_id_, data, true);
    return true;
  }

  void SetDataHandler(
      std::function<void(uint64_t session_id, const std::vector<uint8_t> &data)>
          handler) {
    std::lock_guard<std::mutex> lock(mtx_);
    data_handler_ = std::move(handler);
  }

  void SetDisconnectHandler(std::function<void()> handler) {
    std::lock_guard<std::mutex> lock(mtx_);
    disconnect_handler_ = std::move(handler);
  }

  void StartRequest(uint64_t session_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    active_stream_id_ = client_.OpenStream(true);
    stream_finished_ = false;
    is_h3_ = false;
    headers_sent_ = false;
    resp_headers_.clear();
    recv_buf_.clear();
  }

  void EndRequest(uint64_t session_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    client_.SendOnStream(active_stream_id_, std::vector<uint8_t>{}, true);
  }

  void SetSendChunkSize(size_t size) { send_chunk_size_ = size; }
  void SetMaxOutboundBufferSize(size_t size) {
    max_outbound_buffer_size_ = size;
  }

  cpptcpnet::ConnectionProfile GetConnectionProfile(uint64_t session_id) const {
    cpptcpnet::ConnectionProfile profile;
    profile.max_outbound_buffer_size = max_outbound_buffer_size_;
    profile.send_chunk_size = send_chunk_size_;
    return profile;
  }

  std::shared_ptr<cpppubsub::Subscriber<cpptcpnet::TransferEvent>>
  GetTransferSubscriber() {
    return nullptr;
  }

  cppquic::QuicClient &GetClient() { return client_; }
  const cppquic::QuicClient &GetClient() const { return client_; }

  void SetQuicProfile(const cppquic::QuicProfile &profile) {
    client_.SetQuicProfile(profile);
    if (profile.initial_max_data >= 32 * 1024 * 1024) {
      send_chunk_size_ = 128 * 1024;
    }
  }

 private:
  cppquic::QuicClient client_;
  size_t send_chunk_size_ = 65536;
  size_t max_outbound_buffer_size_ = 1024 * 1024;
  uint64_t active_stream_id_ = 0;
  bool stream_finished_ = false;
  bool is_h3_ = false;
  bool headers_sent_ = false;
  std::function<void(uint64_t session_id, const std::vector<uint8_t> &data)>
      data_handler_;
  std::function<void()> disconnect_handler_;
  std::vector<uint8_t> recv_buf_;
  std::vector<std::pair<std::string, std::string>> resp_headers_;
  std::mutex mtx_;
};

class HybridServerTransport {
 public:
  struct ConnectionEvent {
    uint64_t session_id;
    cpptcpnet::ConnectionState state;
  };
  struct TransferEvent {
    uint64_t session_id;
    bool is_send;
    size_t bytes_transferred;
  };

  struct SessionKey {
    bool is_quic;
    uint64_t transport_session_id;

    bool operator<(const SessionKey &o) const {
      if (is_quic != o.is_quic) return is_quic < o.is_quic;
      return transport_session_id < o.transport_session_id;
    }
  };

  HybridServerTransport(uint16_t port,
                        const std::string &bind_address = "0.0.0.0")
      : tcp_transport_(port, bind_address),
        quic_transport_(port, bind_address) {}

  void Start() {
    tcp_transport_.Start();
    quic_transport_.Start();
  }

  void Stop() {
    tcp_transport_.Stop();
    quic_transport_.Stop();
    std::lock_guard<std::mutex> lock(session_map_mutex_);
    unified_to_transport_.clear();
    transport_to_unified_.clear();
  }

  uint16_t GetLocalPort() const { return tcp_transport_.GetLocalPort(); }

  bool SupportsQuic() const { return true; }

  bool IsQuicSession(uint64_t session_id) const {
    std::lock_guard<std::mutex> lock(session_map_mutex_);
    auto it = unified_to_transport_.find(session_id);
    if (it != unified_to_transport_.end()) {
      return it->second.is_quic;
    }
    return false;
  }

  bool Send(uint64_t session_id, const std::string &data) {
    if (auto opt = GetTransportSession(session_id)) {
      if (opt->is_quic) {
        return quic_transport_.Send(opt->transport_session_id, data);
      } else {
        return tcp_transport_.Send(opt->transport_session_id, data);
      }
    }
    return false;
  }

  bool Send(uint64_t session_id, const std::vector<uint8_t> &data) {
    if (auto opt = GetTransportSession(session_id)) {
      if (opt->is_quic) {
        return quic_transport_.Send(opt->transport_session_id, data);
      } else {
        return tcp_transport_.Send(opt->transport_session_id, data);
      }
    }
    return false;
  }

  void Disconnect(uint64_t session_id) {
    if (auto opt = GetTransportSession(session_id)) {
      if (opt->is_quic) {
        quic_transport_.Disconnect(opt->transport_session_id);
      } else {
        tcp_transport_.Disconnect(opt->transport_session_id);
      }
    }
  }

  cpptcpnet::ConnectionProfile GetConnectionProfile(uint64_t session_id) const {
    if (auto opt = GetTransportSession(session_id)) {
      if (opt->is_quic) {
        return quic_transport_.GetConnectionProfile(opt->transport_session_id);
      } else {
        return tcp_transport_.GetConnectionProfile(opt->transport_session_id);
      }
    }
    return {};
  }

  void ApplyConnectionProfile(uint64_t session_id,
                              const cpptcpnet::ConnectionProfile &profile) {
    if (auto opt = GetTransportSession(session_id)) {
      if (opt->is_quic) {
        quic_transport_.ApplyConnectionProfile(opt->transport_session_id,
                                               profile);
      } else {
        tcp_transport_.ApplyConnectionProfile(opt->transport_session_id,
                                              profile);
      }
    }
  }

  cpptcpnet::PeerAddress GetPeerAddress(uint64_t session_id) const {
    if (auto opt = GetTransportSession(session_id)) {
      if (opt->is_quic) {
        return quic_transport_.GetPeerAddress(opt->transport_session_id);
      } else {
        return tcp_transport_.GetPeerAddress(opt->transport_session_id);
      }
    }
    return {};
  }

  std::optional<TransferEvent> TryReceiveTransferEvent() {
    if (auto ev = tcp_transport_.TryReceiveTransferEvent()) {
      uint64_t unified_id = GetOrCreateUnifiedSessionId(false, ev->session_id);
      return TransferEvent{unified_id, ev->is_send, ev->bytes_transferred};
    }
    if (auto ev = quic_transport_.TryReceiveTransferEvent()) {
      uint64_t unified_id = GetOrCreateUnifiedSessionId(true, ev->session_id);
      return TransferEvent{unified_id, ev->is_send, ev->bytes_transferred};
    }
    return std::nullopt;
  }

  std::optional<ConnectionEvent> TryReceiveConnectionEvent() {
    if (auto ev = tcp_transport_.TryReceiveConnectionEvent()) {
      uint64_t unified_id = GetOrCreateUnifiedSessionId(false, ev->session_id);
      if (ev->state == cpptcpnet::ConnectionState::Disconnected) {
        RemoveSessionMapping(unified_id);
      }
      return ConnectionEvent{unified_id, ev->state};
    }
    if (auto ev = quic_transport_.TryReceiveConnectionEvent()) {
      uint64_t unified_id = GetOrCreateUnifiedSessionId(true, ev->session_id);
      if (ev->state == cpptcpnet::ConnectionState::Disconnected) {
        RemoveSessionMapping(unified_id);
      }
      return ConnectionEvent{unified_id, ev->state};
    }
    return std::nullopt;
  }

  void SetDataHandler(
      std::function<void(uint64_t session_id, const std::vector<uint8_t> &data)>
          handler) {
    data_handler_ = std::move(handler);
    tcp_transport_.SetDataHandler(
        [this](uint64_t session_id, const std::vector<uint8_t> &data) {
          uint64_t unified_id = GetOrCreateUnifiedSessionId(false, session_id);
          if (data_handler_) {
            data_handler_(unified_id, data);
          }
        });
    quic_transport_.SetDataHandler(
        [this](uint64_t session_id, const std::vector<uint8_t> &data) {
          uint64_t unified_id = GetOrCreateUnifiedSessionId(true, session_id);
          if (data_handler_) {
            data_handler_(unified_id, data);
          }
        });
  }

  cppquic::QuicServer &GetServer() { return quic_transport_.GetServer(); }
  const cppquic::QuicServer &GetServer() const {
    return quic_transport_.GetServer();
  }

  void SetSendChunkSize(size_t size) {
    tcp_transport_.SetSendChunkSize(size);
    quic_transport_.SetSendChunkSize(size);
  }

  void SetMaxOutboundBufferSize(size_t size) {
    tcp_transport_.SetMaxOutboundBufferSize(size);
    quic_transport_.SetMaxOutboundBufferSize(size);
  }

  void SetQuicProfile(const cppquic::QuicProfile &profile) {
    quic_transport_.SetQuicProfile(profile);
  }

  TcpServerTransport &GetTcpTransport() { return tcp_transport_; }
  const TcpServerTransport &GetTcpTransport() const { return tcp_transport_; }

  QuicServerTransport &GetQuicTransport() { return quic_transport_; }
  const QuicServerTransport &GetQuicTransport() const {
    return quic_transport_;
  }

 private:
  uint64_t GetOrCreateUnifiedSessionId(bool is_quic,
                                       uint64_t transport_session_id) {
    std::lock_guard<std::mutex> lock(session_map_mutex_);
    SessionKey key{is_quic, transport_session_id};
    auto it = transport_to_unified_.find(key);
    if (it != transport_to_unified_.end()) {
      return it->second;
    }
    static std::atomic<uint64_t> counter{1};
    uint64_t unified_id = counter.fetch_add(1, std::memory_order_relaxed);
    transport_to_unified_[key] = unified_id;
    unified_to_transport_[unified_id] = key;
    return unified_id;
  }

  std::optional<SessionKey> GetTransportSession(
      uint64_t unified_session_id) const {
    std::lock_guard<std::mutex> lock(session_map_mutex_);
    auto it = unified_to_transport_.find(unified_session_id);
    if (it != unified_to_transport_.end()) {
      return it->second;
    }
    return std::nullopt;
  }

  void RemoveSessionMapping(uint64_t unified_session_id) {
    std::lock_guard<std::mutex> lock(session_map_mutex_);
    auto it = unified_to_transport_.find(unified_session_id);
    if (it != unified_to_transport_.end()) {
      transport_to_unified_.erase(it->second);
      unified_to_transport_.erase(it);
    }
  }

  TcpServerTransport tcp_transport_;
  QuicServerTransport quic_transport_;
  std::function<void(uint64_t session_id, const std::vector<uint8_t> &data)>
      data_handler_;
  mutable std::mutex session_map_mutex_;
  std::unordered_map<uint64_t, SessionKey> unified_to_transport_;
  std::map<SessionKey, uint64_t> transport_to_unified_;
};
#endif

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
    if (lhs.size() != rhs.size()) return false;
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

inline std::optional<std::string> gzip_compress(
    const std::string &data, int level = Z_DEFAULT_COMPRESSION) {
  if (data.size() > std::numeric_limits<uInt>::max()) {
    return std::nullopt;
  }
  z_stream zs;
  std::memset(&zs, 0, sizeof(zs));
  if (deflateInit2(&zs, level, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) !=
      Z_OK) {
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

inline std::optional<std::string> brotli_compress(const std::string &data,
                                                  int quality = 4) {
  size_t max_compressed_size = BrotliEncoderMaxCompressedSize(data.size());
  std::string outstring(max_compressed_size, '\0');
  size_t encoded_size = max_compressed_size;

  if (BrotliEncoderCompress(
          quality, BROTLI_DEFAULT_WINDOW, BROTLI_MODE_GENERIC, data.size(),
          reinterpret_cast<const uint8_t *>(data.data()), &encoded_size,
          reinterpret_cast<uint8_t *>(&outstring[0])) == BROTLI_FALSE) {
    return std::nullopt;
  }
  outstring.resize(encoded_size);
  return outstring;
}

inline std::optional<std::string> brotli_decompress(const std::string &data) {
  BrotliDecoderState *state =
      BrotliDecoderCreateInstance(nullptr, nullptr, nullptr);
  if (!state) {
    return std::nullopt;
  }

  size_t available_in = data.size();
  const uint8_t *next_in = reinterpret_cast<const uint8_t *>(data.data());

  std::string outstring;
  char outbuffer[32768];

  BrotliDecoderResult result = BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT;
  while (result == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT) {
    size_t available_out = sizeof(outbuffer);
    uint8_t *next_out = reinterpret_cast<uint8_t *>(outbuffer);

    result = BrotliDecoderDecompressStream(state, &available_in, &next_in,
                                           &available_out, &next_out, nullptr);

    size_t decompressed_size = sizeof(outbuffer) - available_out;
    if (decompressed_size > 0) {
      outstring.append(outbuffer, decompressed_size);
    }

    if (result == BROTLI_DECODER_RESULT_ERROR) {
      BrotliDecoderDestroyInstance(state);
      return std::nullopt;
    }
  }

  BrotliDecoderDestroyInstance(state);

  if (result != BROTLI_DECODER_RESULT_SUCCESS) {
    return std::nullopt;
  }
  return outstring;
}

inline std::optional<std::string> zstd_compress(const std::string &data,
                                                int level = 3) {
  size_t max_compressed_size = ZSTD_compressBound(data.size());
  std::string outstring(max_compressed_size, '\0');

  size_t const compressed_size = ZSTD_compress(
      &outstring[0], max_compressed_size, data.data(), data.size(), level);

  if (ZSTD_isError(compressed_size)) {
    return std::nullopt;
  }
  outstring.resize(compressed_size);
  return outstring;
}

inline std::optional<std::string> zstd_decompress(const std::string &data) {
  unsigned long long const rSize =
      ZSTD_getFrameContentSize(data.data(), data.size());
  if (rSize == ZSTD_CONTENTSIZE_ERROR) {
    return std::nullopt;
  }

  if (rSize != ZSTD_CONTENTSIZE_UNKNOWN) {
    std::string outstring(static_cast<size_t>(rSize), '\0');
    size_t const dSize = ZSTD_decompress(&outstring[0], outstring.size(),
                                         data.data(), data.size());
    if (ZSTD_isError(dSize)) {
      return std::nullopt;
    }
    return outstring;
  }

  ZSTD_DCtx *const dctx = ZSTD_createDCtx();
  if (!dctx) return std::nullopt;

  size_t const buffOutSize = ZSTD_DStreamOutSize();
  std::vector<char> buffOut(buffOutSize);

  std::string outstring;
  ZSTD_inBuffer input = {data.data(), data.size(), 0};

  bool more_output = true;
  while (input.pos < input.size || more_output) {
    ZSTD_outBuffer output = {buffOut.data(), buffOutSize, 0};
    size_t const ret = ZSTD_decompressStream(dctx, &output, &input);
    if (ZSTD_isError(ret)) {
      ZSTD_freeDCtx(dctx);
      return std::nullopt;
    }
    if (output.pos > 0) {
      outstring.append(buffOut.data(), output.pos);
    }
    more_output = (ret > 0 && output.pos == buffOutSize);
    if (ret == 0) {
      break;
    }
    if (input.pos == input.size && !more_output) {
      break;
    }
  }

  ZSTD_freeDCtx(dctx);
  return outstring;
}

inline std::string negotiate_encoding(const std::string &accept_encoding) {
  if (accept_encoding.empty()) {
    return "identity";
  }

  double q_zstd = -1.0;
  double q_br = -1.0;
  double q_gzip = -1.0;
  double q_wildcard = -1.0;
  double q_identity = -1.0;

  std::istringstream ss(accept_encoding);
  std::string token;
  while (std::getline(ss, token, ',')) {
    size_t start = token.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) continue;
    size_t end = token.find_last_not_of(" \t\r\n");
    std::string part = token.substr(start, end - start + 1);

    size_t semi = part.find(';');
    std::string name =
        (semi == std::string::npos) ? part : part.substr(0, semi);

    size_t name_start = name.find_first_not_of(" \t");
    size_t name_end = name.find_last_not_of(" \t");
    if (name_start != std::string::npos && name_end != std::string::npos) {
      name = name.substr(name_start, name_end - name_start + 1);
    }
    std::transform(name.begin(), name.end(), name.begin(), safe_tolower);

    double q = 1.0;
    if (semi != std::string::npos) {
      std::string param = part.substr(semi + 1);
      size_t q_pos = param.find("q=");
      if (q_pos != std::string::npos) {
        try {
          q = std::stod(param.substr(q_pos + 2));
        } catch (...) {
          q = 1.0;
        }
      }
    }

    if (name == "zstd") {
      q_zstd = q;
    } else if (name == "br") {
      q_br = q;
    } else if (name == "gzip") {
      q_gzip = q;
    } else if (name == "*") {
      q_wildcard = q;
    } else if (name == "identity") {
      q_identity = q;
    }
  }

  if (q_wildcard >= 0.0) {
    if (q_zstd < 0.0) q_zstd = q_wildcard;
    if (q_br < 0.0) q_br = q_wildcard;
    if (q_gzip < 0.0) q_gzip = q_wildcard;
  }

  if (q_zstd < 0.0) q_zstd = 0.0;
  if (q_br < 0.0) q_br = 0.0;
  if (q_gzip < 0.0) q_gzip = 0.0;

  bool identity_explicit = (q_identity >= 0.0);
  if (!identity_explicit) {
    q_identity = 0.0001;
  }

  std::string best_encoding = "identity";
  double best_q = q_identity;

  if (q_gzip > 0.0 && q_gzip > best_q) {
    best_q = q_gzip;
    best_encoding = "gzip";
  }
  if (q_br > 0.0 && q_br >= best_q) {
    best_q = q_br;
    best_encoding = "br";
  }
  if (q_zstd > 0.0 && q_zstd >= best_q) {
    best_q = q_zstd;
    best_encoding = "zstd";
  }

  return best_encoding;
}

inline bool is_compressible_mimetype(const std::string &content_type) {
  if (content_type.empty())
    return true;  // Default to compressible if not specified

  std::string ct = content_type;
  std::transform(ct.begin(), ct.end(), ct.begin(), safe_tolower);

  size_t semi = ct.find(';');
  if (semi != std::string::npos) {
    ct = ct.substr(0, semi);
    size_t end = ct.find_last_not_of(" \t");
    if (end != std::string::npos) {
      ct = ct.substr(0, end + 1);
    }
  }

  // Exclude known pre-compressed image formats (except SVG, which is text-based
  // XML)
  if (ct == "image/jpeg" || ct == "image/jpg" || ct == "image/png" ||
      ct == "image/gif" || ct == "image/webp" || ct == "image/heic" ||
      ct == "image/tiff" || ct == "image/x-icon" || ct == "image/bmp") {
    return false;
  }

  // Exclude video & audio formats
  if (ct.rfind("video/", 0) == 0 || ct.rfind("audio/", 0) == 0) {
    return false;
  }

  // Exclude standard archive formats & PDF (which has internal object
  // compression) & generic binary stream
  if (ct == "application/zip" || ct == "application/gzip" ||
      ct == "application/x-gzip" || ct == "application/x-bzip2" ||
      ct == "application/x-tar" || ct == "application/x-rar-compressed" ||
      ct == "application/x-7z-compressed" || ct == "application/x-compress" ||
      ct == "application/pdf" || ct == "application/octet-stream") {
    return false;
  }

  return true;
}

struct StreamCompressor {
  std::string type;
  int level_or_quality;
  bool initialized = false;

  // Gzip state
  z_stream zs;

  // Brotli state
  BrotliEncoderState *br_state = nullptr;

  // Zstd state
  ZSTD_CCtx *zstd_ctx = nullptr;

  StreamCompressor(const std::string &t, int lvl)
      : type(t), level_or_quality(lvl) {}

  ~StreamCompressor() {
    if (initialized) {
      if (type == "gzip") {
        deflateEnd(&zs);
      } else if (type == "br") {
        if (br_state) BrotliEncoderDestroyInstance(br_state);
      } else if (type == "zstd") {
        if (zstd_ctx) ZSTD_freeCCtx(zstd_ctx);
      }
    }
  }

  bool Init() {
    if (type == "gzip") {
      std::memset(&zs, 0, sizeof(zs));
      if (deflateInit2(&zs, level_or_quality, Z_DEFLATED, 15 + 16, 8,
                       Z_DEFAULT_STRATEGY) != Z_OK) {
        return false;
      }
    } else if (type == "br") {
      br_state = BrotliEncoderCreateInstance(nullptr, nullptr, nullptr);
      if (!br_state) return false;
      BrotliEncoderSetParameter(br_state, BROTLI_PARAM_QUALITY,
                                level_or_quality);
    } else if (type == "zstd") {
      zstd_ctx = ZSTD_createCCtx();
      if (!zstd_ctx) return false;
      ZSTD_CCtx_setParameter(zstd_ctx, ZSTD_c_compressionLevel,
                             level_or_quality);
    }
    initialized = true;
    return true;
  }

  std::optional<std::string> Compress(const std::string &input_data,
                                      bool is_last) {
    if (!initialized && !Init()) return std::nullopt;

    std::string output;
    char outbuffer[65536];

    if (type == "gzip") {
      zs.next_in =
          reinterpret_cast<Bytef *>(const_cast<char *>(input_data.data()));
      zs.avail_in = static_cast<uInt>(input_data.size());

      int flush = is_last ? Z_FINISH : Z_SYNC_FLUSH;
      int ret;
      do {
        zs.next_out = reinterpret_cast<Bytef *>(outbuffer);
        zs.avail_out = sizeof(outbuffer);
        ret = deflate(&zs, flush);
        if (ret == Z_BUF_ERROR) {
          break;
        }
        if (ret < 0) {
          return std::nullopt;
        }
        size_t written = sizeof(outbuffer) - zs.avail_out;
        if (written > 0) {
          output.append(outbuffer, written);
        }
      } while (zs.avail_out == 0 || (is_last && ret != Z_STREAM_END));

      if (is_last && ret != Z_STREAM_END) {
        return std::nullopt;
      }
      return output;
    } else if (type == "br") {
      size_t avail_in = input_data.size();
      const uint8_t *next_in =
          reinterpret_cast<const uint8_t *>(input_data.data());

      BrotliEncoderOperation op =
          is_last ? BROTLI_OPERATION_FINISH : BROTLI_OPERATION_FLUSH;
      while (true) {
        size_t avail_out = sizeof(outbuffer);
        uint8_t *next_out = reinterpret_cast<uint8_t *>(outbuffer);
        if (BrotliEncoderCompressStream(br_state, op, &avail_in, &next_in,
                                        &avail_out, &next_out,
                                        nullptr) == BROTLI_FALSE) {
          return std::nullopt;
        }
        size_t written = sizeof(outbuffer) - avail_out;
        if (written > 0) {
          output.append(reinterpret_cast<char *>(outbuffer), written);
        }
        if (is_last) {
          if (BrotliEncoderIsFinished(br_state) == BROTLI_TRUE) {
            break;
          }
        } else {
          if (avail_in == 0 &&
              BrotliEncoderHasMoreOutput(br_state) == BROTLI_FALSE) {
            break;
          }
        }
      }
      return output;
    } else if (type == "zstd") {
      ZSTD_inBuffer input = {input_data.data(), input_data.size(), 0};
      ZSTD_EndDirective reset = is_last ? ZSTD_e_end : ZSTD_e_flush;
      bool finished = false;
      while (!finished) {
        ZSTD_outBuffer out = {outbuffer, sizeof(outbuffer), 0};
        size_t const ret = ZSTD_compressStream2(zstd_ctx, &out, &input, reset);
        if (ZSTD_isError(ret)) {
          return std::nullopt;
        }
        if (out.pos > 0) {
          output.append(outbuffer, out.pos);
        }
        if (is_last) {
          finished = (ret == 0);
        } else {
          finished = (input.pos == input.size && out.pos < sizeof(outbuffer));
        }
      }
      return output;
    }
    return std::nullopt;
  }
};

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
  if (boundary.empty()) return parts;

  std::string delimiter = "--" + boundary;
  std::string end_delimiter = delimiter + "--";

  size_t pos = 0;
  while (true) {
    pos = body.find(delimiter, pos);
    if (pos == std::string::npos) break;

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
      if (next_delim == std::string::npos) break;
      if (next_delim > 0 && body[next_delim - 1] == '\n') {
        break;
      }
      next_delim += delimiter.length();
    }

    if (next_delim == std::string::npos) break;

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
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

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
    if (idx >= buffer.size()) return false;

    auto it_crlf = std::search(buffer.begin() + idx, buffer.end(),
                               reinterpret_cast<const uint8_t *>("\r\n"),
                               reinterpret_cast<const uint8_t *>("\r\n") + 2);
    if (it_crlf == buffer.end()) return false;

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
      if (double_crlf == buffer.end()) return false;
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
inline bool match_route(
    const std::string &request_method,
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
          if (!remaining.empty()) remaining += "/";
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
  size_t min_header_len = 2 + (frame.masked ? 4 : 0);
  if (payload_len == 126) {
    if (buffer.size() < min_header_len + 2) return false;
    payload_len =
        (static_cast<uint64_t>(static_cast<uint8_t>(buffer[2])) << 8) |
        static_cast<uint8_t>(buffer[3]);
    header_len = 4;
  } else if (payload_len == 127) {
    if (buffer.size() < min_header_len + 8) return false;
    payload_len = 0;
    for (int i = 0; i < 8; ++i) {
      payload_len = (payload_len << 8) | static_cast<uint8_t>(buffer[2 + i]);
    }
    header_len = 10;
  } else {
    if (buffer.size() < min_header_len) return false;
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
    if (buffer.size() < header_len + 4) return false;
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
  virtual ~WebSocketConnection() = default;
  virtual uint64_t GetSessionId() const = 0;
  virtual bool Send(const std::string &message) = 0;
  virtual bool SendBinary(const std::vector<uint8_t> &data) = 0;
  virtual bool Ping(const std::string &payload = "") = 0;
  virtual void Close(uint16_t status_code = 1000,
                     const std::string &reason = "") = 0;
};

template <typename Transport>
class WebSocketConnectionImpl : public WebSocketConnection {
 public:
  WebSocketConnectionImpl(uint64_t session_id,
                          std::shared_ptr<Transport> transport)
      : session_id_(session_id), transport_(transport) {}

  uint64_t GetSessionId() const override { return session_id_; }

  bool Send(const std::string &message) override {
    auto frame = CreateWsFrame(0x01, message);
    if (auto t = transport_.lock()) {
      return t->Send(session_id_, frame);
    }
    return false;
  }

  bool SendBinary(const std::vector<uint8_t> &data) override {
    auto frame = CreateWsFrame(0x02, data);
    if (auto t = transport_.lock()) {
      return t->Send(session_id_, frame);
    }
    return false;
  }

  bool Ping(const std::string &payload = "") override {
    auto frame = CreateWsFrame(0x09, payload);
    if (auto t = transport_.lock()) {
      return t->Send(session_id_, frame);
    }
    return false;
  }

  void Close(uint16_t status_code = 1000,
             const std::string &reason = "") override {
    std::vector<uint8_t> payload;
    payload.push_back(static_cast<uint8_t>((status_code >> 8) & 0xFF));
    payload.push_back(static_cast<uint8_t>(status_code & 0xFF));
    payload.insert(payload.end(), reason.begin(), reason.end());
    auto frame = CreateWsFrame(0x08, payload);
    if (auto t = transport_.lock()) {
      t->Send(session_id_, frame);
    }
  }

 private:
  uint64_t session_id_;
  std::weak_ptr<Transport> transport_;
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
    p.idle_timeout = std::chrono::milliseconds(3600000);  // 1 hour idle timeout
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
  size_t file_offset = 0;
  size_t file_length = 0;

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
  if (dot_pos == std::string::npos) return "application/octet-stream";
  std::string ext = filepath.substr(dot_pos + 1);
  std::transform(ext.begin(), ext.end(), ext.begin(), safe_tolower);
  if (ext == "html" || ext == "htm") return "text/html";
  if (ext == "css") return "text/css";
  if (ext == "js" || ext == "mjs") return "application/javascript";
  if (ext == "json") return "application/json";
  if (ext == "png") return "image/png";
  if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
  if (ext == "gif") return "image/gif";
  if (ext == "svg") return "image/svg+xml";
  if (ext == "txt") return "text/plain";
  if (ext == "ico") return "image/x-icon";
  if (ext == "xml") return "application/xml";
  if (ext == "pdf") return "application/pdf";
  if (ext == "wasm") return "application/wasm";
  if (ext == "yaml" || ext == "yml") return "application/x-yaml";
  if (ext == "md") return "text/markdown";
  if (ext == "csv") return "text/csv";
  if (ext == "woff") return "font/woff";
  if (ext == "woff2") return "font/woff2";
  if (ext == "ttf") return "font/ttf";
  if (ext == "otf") return "font/otf";
  if (ext == "eot") return "application/vnd.ms-fontobject";
  if (ext == "webmanifest") return "application/manifest+json";
  if (ext == "webp") return "image/webp";
  if (ext == "bmp") return "image/bmp";
  if (ext == "tiff" || ext == "tif") return "image/tiff";
  if (ext == "heic") return "image/heic";
  if (ext == "heif") return "image/heif";
  if (ext == "map") return "application/json";
  if (ext == "xhtml") return "application/xhtml+xml";
  if (ext == "zip") return "application/zip";
  if (ext == "gz") return "application/gzip";
  if (ext == "tgz") return "application/gzip";
  if (ext == "tar") return "application/x-tar";
  if (ext == "rar") return "application/x-rar-compressed";
  if (ext == "7z") return "application/x-7z-compressed";
  if (ext == "doc") return "application/msword";
  if (ext == "docx")
    return "application/"
           "vnd.openxmlformats-officedocument.wordprocessingml.document";
  if (ext == "xls") return "application/vnd.ms-excel";
  if (ext == "xlsx")
    return "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
  if (ext == "ppt") return "application/vnd.ms-powerpoint";
  if (ext == "pptx")
    return "application/"
           "vnd.openxmlformats-officedocument.presentationml.presentation";
  if (ext == "rtf") return "application/rtf";
  if (ext == "ini" || ext == "conf" || ext == "config" || ext == "log")
    return "text/plain";
  if (ext == "sh") return "application/x-sh";
  if (ext == "bat") return "text/plain";
  if (ext == "mp4") return "video/mp4";
  if (ext == "webm") return "video/webm";
  if (ext == "mp3") return "audio/mpeg";
  if (ext == "ogg") return "audio/ogg";
  if (ext == "wav") return "audio/wav";
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
  std::chrono::seconds max_age = std::chrono::seconds(86400);  // 24 hours
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
    if (vec.empty()) return "";
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
      : max_requests_(max_requests),
        window_duration_(window_duration),
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
    return false;  // Stop further processing
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

  std::function<HttpResponse(const HttpRequest &)> Wrap(
      std::function<HttpResponse(const HttpRequest &)> handler,
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
template <typename Transport = TcpServerTransport>
class HttpServerImpl {
 public:
  using RouteHandler = std::function<HttpResponse(const HttpRequest &)>;
  using Middleware = std::function<bool(HttpRequest &, HttpResponse &)>;
  using StreamHandler = std::function<std::optional<HttpResponse>(
      const HttpRequest &, const std::string &, bool)>;

  HttpServerImpl(const HttpServerImpl &) = delete;
  HttpServerImpl &operator=(const HttpServerImpl &) = delete;
  HttpServerImpl(HttpServerImpl &&) = delete;
  HttpServerImpl &operator=(HttpServerImpl &&) = delete;

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
    size_t file_bytes_remaining = std::string::npos;
    bool should_close_after_file = false;
    bool is_compressed_stream = false;
    bool compressed_stream_finished = false;
    std::shared_ptr<StreamCompressor> compressor;

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
   * @brief Constructs a new HttpServerImpl.
   * @param port The port to listen on.
   * @param bind_address The IP address to bind the listener to (defaults to
   * "0.0.0.0").
   */
  HttpServerImpl(uint16_t port, const std::string &bind_address = "0.0.0.0")
      : transport_(std::make_shared<Transport>(port, bind_address)) {
    transport_->SetDataHandler(
        [this](uint64_t session_id, const std::vector<uint8_t> &data) {
          this->HandleIncomingData(session_id, data);
        });
  }

  ~HttpServerImpl() {
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
           canonical_dir.back() != '/' && canonical_dir.back() != '\\' &&
           canonical_filepath[canonical_dir.size()] != '/' &&
           canonical_filepath[canonical_dir.size()] != '\\')) {
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
      size_t file_size = std::filesystem::file_size(canonical_filepath, ec);
      size_t start = 0;
      size_t end = file_size > 0 ? file_size - 1 : 0;
      bool is_partial = false;

      std::string range_header = req.GetHeader("Range");
      if (!range_header.empty() && range_header.find("bytes=") == 0) {
        std::string range_str = range_header.substr(6);
        size_t dash_pos = range_str.find('-');
        if (dash_pos != std::string::npos) {
          std::string start_str = range_str.substr(0, dash_pos);
          std::string end_str = range_str.substr(dash_pos + 1);

          try {
            if (start_str.empty()) {
              if (!end_str.empty()) {
                size_t suffix_length = std::stoull(end_str);
                if (suffix_length > 0) {
                  start =
                      suffix_length > file_size ? 0 : file_size - suffix_length;
                  end = file_size > 0 ? file_size - 1 : 0;
                  is_partial = true;
                }
              }
            } else {
              start = std::stoull(start_str);
              if (!end_str.empty()) end = std::stoull(end_str);

              if (start <= end && start < file_size) {
                if (end >= file_size) end = file_size - 1;
                is_partial = true;
              } else {
                res.status_code = 416;
                res.status_message = "Range Not Satisfiable";
                res.headers["Content-Range"] =
                    "bytes */" + std::to_string(file_size);
                return res;
              }
            }
          } catch (const std::exception &) {
            // Ignore malformed range header and fall back to 200 OK
          }
        }
      }

      if (is_partial) {
        res.status_code = 206;
        res.status_message = "Partial Content";
        res.file_offset = start;
        res.file_length = end - start + 1;
        res.headers["Content-Range"] = "bytes " + std::to_string(start) + "-" +
                                       std::to_string(end) + "/" +
                                       std::to_string(file_size);
        res.headers["Content-Length"] = std::to_string(res.file_length);
      } else {
        res.status_code = 200;
        res.status_message = "OK";
        res.headers["Content-Length"] = std::to_string(file_size);
      }

      res.is_file = true;
      res.file_path = canonical_filepath;
      res.headers["Content-Type"] = GetMimeType(canonical_filepath);
      res.headers["Accept-Ranges"] = "bytes";
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
          if (auto event = transport_->TryReceiveTransferEvent()) {
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
                    transport_->Disconnect(event->session_id);
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

          if (auto event = transport_->TryReceiveConnectionEvent()) {
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
                  try {
                    session_to_close->ws_behavior.on_close(
                        session_to_close->ws_connection);
                  } catch (...) {
                    // Prevent user callback exceptions from crashing the
                    // background event thread.
                  }
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
    transport_->Start();
  }

  void Stop() {
    std::thread thread_to_join;
    {
      std::lock_guard<std::mutex> lock(shutdown_mutex_);
      if (!running_) {
        transport_->Stop();
        return;
      }
      running_ = false;
      shutdown_cv_.notify_all();
      thread_to_join = std::move(background_thread_);
    }
    if (thread_to_join.joinable()) {
      thread_to_join.join();
    }
    transport_->Stop();
    {
      std::lock_guard<std::mutex> lock(session_mutex_);
      sessions_.clear();
    }
  }

  auto &GetListener() { return transport_->GetListener(); }
  const auto &GetListener() const { return transport_->GetListener(); }
  std::shared_ptr<Transport> GetTransport() { return transport_; }
  std::shared_ptr<const Transport> GetTransport() const { return transport_; }

  void SetMaxHeaderSize(size_t size) { max_header_size_ = size; }
  size_t GetMaxHeaderSize() const { return max_header_size_; }

  void SetMaxBodySize(size_t size) { max_body_size_ = size; }
  size_t GetMaxBodySize() const { return max_body_size_; }

  void SetSendChunkSize(size_t size) {
    if (transport_) {
      transport_->SetSendChunkSize(size);
    }
  }

  void SetMaxOutboundBufferSize(size_t size) {
    if (transport_) {
      transport_->SetMaxOutboundBufferSize(size);
    }
  }

#ifdef CPPHTTP_ENABLE_QUIC
  void SetQuicProfile(const cppquic::QuicProfile &profile) {
    if (transport_) {
      transport_->SetQuicProfile(profile);
    }
  }
#endif

  void SetIdleTimeout(std::chrono::milliseconds timeout) {
    idle_timeout_ = timeout;
  }
  std::chrono::milliseconds GetIdleTimeout() const { return idle_timeout_; }

  void SetBrotliQuality(int quality) {
    if (quality < 0 || quality > 11) {
      throw std::invalid_argument(
          "Brotli quality must be in the range [0, 11]");
    }
    brotli_quality_ = quality;
  }
  int GetBrotliQuality() const { return brotli_quality_; }

  void SetZstdCompressionLevel(int level) {
    if (level < ZSTD_minCLevel() || level > ZSTD_maxCLevel()) {
      throw std::invalid_argument("Zstd level must be in the range [" +
                                  std::to_string(ZSTD_minCLevel()) + ", " +
                                  std::to_string(ZSTD_maxCLevel()) + "]");
    }
    zstd_level_ = level;
  }
  int GetZstdCompressionLevel() const { return zstd_level_; }

  void SetGzipCompressionLevel(int level) {
    if (level < -1 || level > 9) {
      throw std::invalid_argument("Gzip level must be in the range [-1, 9]");
    }
    gzip_level_ = level;
  }
  int GetGzipCompressionLevel() const { return gzip_level_; }

  void SetMinCompressionSize(size_t size) { min_compression_size_ = size; }
  size_t GetMinCompressionSize() const { return min_compression_size_; }

  void SetAdvertiseAltSvc(bool enable) { advertise_alt_svc_ = enable; }
  bool GetAdvertiseAltSvc() const { return advertise_alt_svc_; }

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

  std::shared_ptr<Transport> transport_;
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
  int brotli_quality_ = 4;
  int zstd_level_ = 3;
  int gzip_level_ = Z_DEFAULT_COMPRESSION;
  size_t min_compression_size_ = 1024;
  bool advertise_alt_svc_ = true;

  void PumpFileTransfer(uint64_t session_id,
                        std::shared_ptr<SessionState> session) {
    if (!session->is_streaming_file || !session->file_stream ||
        !session->file_stream->is_open())
      return;

    size_t max_flight = 65536;
    size_t chunk_size = 65536;

    try {
      auto profile = transport_->GetConnectionProfile(session_id);
      max_flight = profile.max_outbound_buffer_size / 2;
      chunk_size = profile.send_chunk_size;
    } catch (const std::exception &) {
      // The session has been closed/disconnected during the file transfer pump.
      // Cancel the streaming file transfer gracefully.
      session->is_streaming_file = false;
      session->file_stream.reset();
      return;
    }

    if (max_flight < 65536) max_flight = 65536;
    if (chunk_size == 0) chunk_size = 65536;

    while (session->file_bytes_in_flight < max_flight &&
           *session->file_stream && running_) {
      size_t to_read = chunk_size;
      if (session->file_bytes_remaining != std::string::npos &&
          session->file_bytes_remaining < chunk_size) {
        to_read = session->file_bytes_remaining;
      }
      if (to_read == 0) break;

      std::vector<char> stream_buffer(to_read);
      session->file_stream->read(stream_buffer.data(), stream_buffer.size());
      std::streamsize bytes_read = session->file_stream->gcount();
      if (bytes_read > 0) {
        if (session->file_bytes_remaining != std::string::npos) {
          session->file_bytes_remaining -= bytes_read;
        }

        std::string raw_chunk(stream_buffer.data(), bytes_read);

        if (session->is_compressed_stream) {
          bool is_eof =
              (!*session->file_stream || session->file_stream->eof() ||
               session->file_bytes_remaining == 0);
          auto compressed_opt =
              session->compressor->Compress(raw_chunk, is_eof);
          if (!compressed_opt.has_value()) {
            session->is_streaming_file = false;
            session->file_stream.reset();
            session->compressor.reset();
            return;
          }
          std::string compressed = std::move(*compressed_opt);
          if (!compressed.empty()) {
            std::ostringstream chunk_stream;
            chunk_stream << std::hex << compressed.size() << "\r\n"
                         << compressed << "\r\n";
            std::string formatted_chunk = chunk_stream.str();
            if (transport_->Send(session_id, formatted_chunk)) {
              session->file_bytes_in_flight += formatted_chunk.size();
            } else {
              break;
            }
          }
          if (is_eof) {
            std::string final_chunk = "0\r\n\r\n";
            if (transport_->Send(session_id, final_chunk)) {
              session->file_bytes_in_flight += final_chunk.size();
            }
            session->compressed_stream_finished = true;
            break;
          }
        } else {
          if (transport_->Send(session_id, raw_chunk)) {
            session->file_bytes_in_flight += bytes_read;
          } else {
            break;
          }
        }
      } else {
        break;
      }
    }

    bool is_done =
        (!*session->file_stream || !session->file_stream->is_open() ||
         session->file_stream->eof() || session->file_bytes_remaining == 0);

    if (is_done && session->is_compressed_stream &&
        !session->compressed_stream_finished) {
      auto compressed_opt = session->compressor->Compress("", true);
      if (compressed_opt.has_value()) {
        std::string compressed = std::move(*compressed_opt);
        if (!compressed.empty()) {
          std::ostringstream chunk_stream;
          chunk_stream << std::hex << compressed.size() << "\r\n"
                       << compressed << "\r\n";
          std::string formatted_chunk = chunk_stream.str();
          if (transport_->Send(session_id, formatted_chunk)) {
            session->file_bytes_in_flight += formatted_chunk.size();
          }
        }
        std::string final_chunk = "0\r\n\r\n";
        if (transport_->Send(session_id, final_chunk)) {
          session->file_bytes_in_flight += final_chunk.size();
        }
      }
      session->compressed_stream_finished = true;
    }

    if (is_done && (!session->is_compressed_stream ||
                    session->compressed_stream_finished)) {
      if (session->file_bytes_in_flight == 0) {
        session->is_streaming_file = false;
        session->file_stream.reset();
        session->compressor.reset();
        if (session->should_close_after_file) {
          transport_->Disconnect(session_id);
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
            if (handler_response) return;
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
        transport_->Send(session_id, response_data);
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
        transport_->Send(session_id, response_data);
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
        std::string chunk(
            buffer.begin() + session->body_start_idx,
            buffer.begin() + session->body_start_idx + to_process);
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
          transport_->Send(session_id, headers_data);
          if (session->request.method != "HEAD") {
            session->is_streaming_file = true;
            session->file_stream = std::make_shared<std::ifstream>(
                handler_response->file_path, std::ios::binary);
            if (handler_response->file_offset > 0) {
              session->file_stream->seekg(handler_response->file_offset);
            }
            session->file_bytes_remaining = handler_response->file_length > 0
                                                ? handler_response->file_length
                                                : std::string::npos;
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
          transport_->Send(session_id, response_data);
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
    buffer.insert(buffer.end(), data.begin(), data.end());

    size_t search_start = 0;
    const std::string terminator = "\r\n\r\n";

    if (!session->is_websocket) {
      auto it = std::search(buffer.begin(), buffer.end(), terminator.begin(),
                            terminator.end());
      if (it == buffer.end()) {
        if (buffer.size() > max_header_size_) {
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
          transport_->Send(session_id, response_data);
          return;
        }
      }
    }

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
            transport_->Send(session_id, pong_frame);
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

      if (header_end_idx > max_header_size_) {
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
        transport_->Send(session_id, response_data);
        return;
      }

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
            auto peer_addr = transport_->GetPeerAddress(session_id);
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
            transport_->Send(session_id, response_data);
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
        transport_->Send(session_id, response_data);
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
          auto peer_addr = transport_->GetPeerAddress(session_id);
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
          transport_->Send(session_id, response_data);
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
            transport_->Send(session_id, response_data);
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
            transport_->Send(session_id, response_data);
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
              transport_->Send(session_id, response_data);
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
            transport_->Send(session_id, response_data);
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

        if (advertise_alt_svc_ && transport_->SupportsQuic() &&
            !transport_->IsQuicSession(session_id)) {
          if (response.headers.find("Alt-Svc") == response.headers.end()) {
            response.headers["Alt-Svc"] =
                "h3=\":" + std::to_string(transport_->GetLocalPort()) +
                "\"; ma=86400";
          }
        }

        if (should_close && !upgrade_to_ws) {
          response.headers["Connection"] = "close";
        }

        std::string accept_encoding = request.GetHeader("Accept-Encoding");
        std::string best_encoding = negotiate_encoding(accept_encoding);

        // For file responses, check if we should compress them.
        // We only compress full file transfers (status 200 OK) with
        // compressible MIME types.
        if (response.is_file && response.status_code == 200 &&
            best_encoding != "identity" &&
            response.headers.count("Content-Encoding") == 0 &&
            is_compressible_mimetype(response.GetHeader("Content-Type"))) {
          std::error_code size_ec;
          size_t file_size =
              std::filesystem::file_size(response.file_path, size_ec);
          if (!size_ec && file_size >= min_compression_size_) {
            response.headers["Content-Encoding"] = best_encoding;
            response.headers["Transfer-Encoding"] = "chunked";
            response.headers.erase("content-length");
          }
        }

        if (best_encoding != "identity" &&
            response.body.size() >= min_compression_size_ &&
            response.headers.count("Content-Encoding") == 0 &&
            is_compressible_mimetype(response.GetHeader("Content-Type"))) {
          std::optional<std::string> compressed;
          if (best_encoding == "zstd") {
            compressed = zstd_compress(response.body, zstd_level_);
          } else if (best_encoding == "br") {
            compressed = brotli_compress(response.body, brotli_quality_);
          } else if (best_encoding == "gzip") {
            compressed = gzip_compress(response.body, gzip_level_);
          }
          if (compressed.has_value()) {
            response.body = std::move(*compressed);
            response.headers["Content-Encoding"] = best_encoding;
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
        size_t total_sent =
            response.is_file
                ? (headers_data.size() +
                   (request.method != "HEAD"
                        ? (response.file_length > 0 ? response.file_length
                                                    : file_size)
                        : 0))
                : response_data.size();

        if (should_close && !upgrade_to_ws && !response.is_file) {
          std::lock_guard<std::mutex> lock(close_mutex_);
          sessions_to_close_.insert(session_id);
          pending_send_bytes_[session_id] = total_sent;
        }

        if (response.is_file) {
          transport_->Send(session_id, headers_data);
          if (request.method != "HEAD") {
            session->is_streaming_file = true;
            session->file_stream = std::make_shared<std::ifstream>(
                response.file_path, std::ios::binary);
            if (response.file_offset > 0) {
              session->file_stream->seekg(response.file_offset);
            }
            session->file_bytes_remaining = response.file_length > 0
                                                ? response.file_length
                                                : std::string::npos;
            session->file_bytes_in_flight = headers_data.size();
            session->should_close_after_file = should_close && !upgrade_to_ws;

            std::string content_encoding = "";
            auto ce_it = response.headers.find("Content-Encoding");
            if (ce_it != response.headers.end()) {
              content_encoding = ce_it->second;
            }
            if (!content_encoding.empty() && content_encoding != "identity") {
              int level = 3;
              if (content_encoding == "zstd")
                level = zstd_level_;
              else if (content_encoding == "br")
                level = brotli_quality_;
              else if (content_encoding == "gzip")
                level = gzip_level_;
              session->compressor =
                  std::make_shared<StreamCompressor>(content_encoding, level);
              session->is_compressed_stream = true;
              session->compressed_stream_finished = false;
            } else {
              session->is_compressed_stream = false;
              session->compressor.reset();
            }

            PumpFileTransfer(session_id, session);
          }
        } else {
          transport_->Send(session_id, response_data);
        }

        if (upgrade_to_ws) {
          transport_->ApplyConnectionProfile(session_id,
                                             ws_behavior.connection_profile);
          session->is_websocket = true;
          session->ws_connection =
              std::make_shared<WebSocketConnectionImpl<Transport>>(session_id,
                                                                   transport_);
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
        transport_->Send(session_id, response_data);
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
      if (!line.empty() && line.back() == '\r') line.pop_back();

      std::istringstream line_stream(line);
      line_stream >> req.method >> req.path >> req.version;
    }

    while (std::getline(stream, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (line.empty()) break;

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
            if (comma == std::string::npos) break;
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

using HttpServer = HttpServerImpl<TcpServerTransport>;

#ifdef CPPHTTP_ENABLE_QUIC
using QuicHttpServer = HttpServerImpl<QuicServerTransport>;
using HybridHttpServer = HttpServerImpl<HybridServerTransport>;
#endif

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

template <typename Transport = TcpClientTransport>
class HttpClientImpl {
 public:
  HttpClientImpl(const std::string &host, uint16_t port,
                 std::shared_ptr<cppasyncworker::WorkerPool> pool = nullptr)
      : host_(host), port_(port), pool_(pool ? pool : GetClientWorkerPool()) {}

  HttpClientImpl(const HttpClientImpl &) = delete;
  HttpClientImpl &operator=(const HttpClientImpl &) = delete;
  HttpClientImpl(HttpClientImpl &&) = delete;
  HttpClientImpl &operator=(HttpClientImpl &&) = delete;

  ~HttpClientImpl() {
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

  HttpResponse Get(
      const std::string &path,
      const std::unordered_map<std::string, std::string> &headers = {}) {
    return SendRequest("GET", path, "", headers);
  }

  HttpResponse Post(
      const std::string &path, const std::string &body,
      const std::unordered_map<std::string, std::string> &headers = {}) {
    return SendRequest("POST", path, body, headers);
  }

  HttpResponse Put(
      const std::string &path, const std::string &body,
      const std::unordered_map<std::string, std::string> &headers = {}) {
    return SendRequest("PUT", path, body, headers);
  }

  HttpResponse Delete(
      const std::string &path,
      const std::unordered_map<std::string, std::string> &headers = {}) {
    return SendRequest("DELETE", path, "", headers);
  }

  std::future<HttpResponse> GetAsync(
      const std::string &path,
      const std::unordered_map<std::string, std::string> &headers = {}) {
    return SendRequestAsync("GET", path, "", headers);
  }

  std::future<HttpResponse> PostAsync(
      const std::string &path, const std::string &body,
      const std::unordered_map<std::string, std::string> &headers = {}) {
    return SendRequestAsync("POST", path, body, headers);
  }

  std::future<HttpResponse> PutAsync(
      const std::string &path, const std::string &body,
      const std::unordered_map<std::string, std::string> &headers = {}) {
    return SendRequestAsync("PUT", path, body, headers);
  }

  std::future<HttpResponse> DeleteAsync(
      const std::string &path,
      const std::unordered_map<std::string, std::string> &headers = {}) {
    return SendRequestAsync("DELETE", path, "", headers);
  }

  HttpResponse GetStream(
      const std::string &path,
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

  std::future<HttpResponse> PostStreamAsync(
      const std::string &path,
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
    std::weak_ptr<std::atomic<bool>> alive_weak = alive_;

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

  std::future<HttpResponse> PutStreamAsync(
      const std::string &path,
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
    std::weak_ptr<std::atomic<bool>> alive_weak = alive_;

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
    std::weak_ptr<std::atomic<bool>> alive_weak = alive_;

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

  void SetDisconnectHandler(std::function<void()> handler) {
    std::lock_guard<std::mutex> lock(mtx_);
    disconnect_handler_ = std::move(handler);
  }

  void StartRequest(uint64_t session_id) {}
  void EndRequest(uint64_t session_id) {}

  cpptcpnet::ConnectionProfile GetConnectionProfile(uint64_t session_id) const {
    std::lock_guard<std::mutex> lock(*const_cast<std::mutex *>(&mtx_));
    return client_.GetConnectionProfile(session_id);
  }

  std::shared_ptr<cpppubsub::Subscriber<cpptcpnet::TransferEvent>>
  GetTransferSubscriber() {
    std::lock_guard<std::mutex> lock(mtx_);
    return client_.GetTransferSubscriber();
  }

  Transport &GetTransport() { return client_; }
  const Transport &GetTransport() const { return client_; }

  void SetSendChunkSize(size_t size) { client_.SetSendChunkSize(size); }
  void SetMaxOutboundBufferSize(size_t size) {
    client_.SetMaxOutboundBufferSize(size);
  }

#ifdef CPPHTTP_ENABLE_QUIC
  void SetQuicProfile(const cppquic::QuicProfile &profile) {
    client_.SetQuicProfile(profile);
  }
#endif

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

  std::shared_ptr<std::atomic<bool>> alive_ =
      std::make_shared<std::atomic<bool>>(true);
  std::shared_ptr<cppasyncworker::WorkerPool> pool_;

  std::string host_;
  uint16_t port_;
  bool ssl_enabled_ = false;
  std::chrono::milliseconds timeout_{10000};
  size_t max_body_size_ = 10 * 1024 * 1024;
  size_t max_header_size_ = 8192;
  int max_redirects_ = 5;

  Transport client_;
  uint64_t session_id_ = 0;
  std::atomic<bool> connected_{false};
  std::atomic<bool> disconnected_{false};
  std::deque<uint8_t> buffer_;
  std::mutex mtx_;
  std::condition_variable cv_;
  std::mutex request_mutex_;
  std::function<void()> disconnect_handler_;

  std::vector<std::shared_future<void>> pending_tasks_;
  std::mutex tasks_mutex_;

  void CleanUpConnection() {
    client_.Stop();
    session_id_ = 0;
    connected_ = false;
    disconnected_ = false;
    buffer_.clear();
  }

  void EnsureConnected() {
    if (connected_ && !disconnected_ && session_id_ != 0) {
      return;
    }

    CleanUpConnection();

    if (ssl_enabled_) {
      client_.EnableSSL();
    }

    client_.SetDataHandler([this](uint64_t session_id,
                                  const std::vector<uint8_t> &data) {
      std::lock_guard<std::mutex> lock(mtx_);
      if (session_id == 0 ||
          (session_id_ != 0 && session_id != session_id_ && session_id != 1)) {
        return;
      }

      if (buffer_.size() + data.size() > max_header_size_ + max_body_size_) {
        internal::Log(
            LogSeverity::Error, "HttpClient",
            "[CLIENT RECV FAIL] Response size exceeded configured limits");
        disconnected_ = true;
        cv_.notify_all();
        return;
      }

      const std::string terminator = "\r\n\r\n";
      auto it = std::search(buffer_.begin(), buffer_.end(), terminator.begin(),
                            terminator.end());
      if (it == buffer_.end() &&
          (buffer_.size() + data.size() > max_header_size_)) {
        internal::Log(
            LogSeverity::Error, "HttpClient",
            "[CLIENT HEADER RECV FAIL] Header size exceeded configured limits");
        disconnected_ = true;
        cv_.notify_all();
        return;
      }

      buffer_.insert(buffer_.end(), data.begin(), data.end());
      cv_.notify_all();
    });

    client_.SetDisconnectHandler([this]() {
      std::lock_guard<std::mutex> lock(mtx_);
      disconnected_ = true;
      connected_ = false;
      cv_.notify_all();
      if (disconnect_handler_) disconnect_handler_();
    });

    uint64_t sid = client_.Connect(host_, port_, timeout_);
    if (sid == 0) {
      CleanUpConnection();
      throw std::runtime_error("Failed to connect to server");
    }

    session_id_ = sid;
    connected_ = true;
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
      if (response_ready) return true;

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
          std::function<void(const std::string &)> chunk_cb;
          if (on_chunk) {
            chunk_cb = [&](const std::string &chunk) {
              HttpResponse temp_res;
              temp_res.status_code = status_code;
              temp_res.status_message = req.status_message;
              temp_res.headers = req.headers;
              temp_res.multi_headers = req.multi_headers;
              on_chunk(temp_res, chunk, false);
            };
          }
          bool completed = ParseChunkedBodyIncremental(
              buffer_, client_chunk_parsed_up_to, http_response.body,
              chunked_bad_request, max_body_size_, chunked_payload_too_large,
              chunk_cb);

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
                std::string chunk(
                    buffer_.begin() + body_start_idx,
                    buffer_.begin() + body_start_idx + to_process);
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

    try {
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
    } catch (...) {
      if (lock.owns_lock()) {
        lock.unlock();
      }
      CleanUpConnection();
      throw;
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
    size_t start_idx = content_encoding.find_first_not_of(" \t\r\n");
    if (start_idx != std::string::npos) {
      size_t end_idx = content_encoding.find_last_not_of(" \t\r\n");
      content_encoding =
          content_encoding.substr(start_idx, end_idx - start_idx + 1);
    }
    if (!http_response.body.empty()) {
      std::optional<std::string> decompressed;
      if (content_encoding == "zstd") {
        decompressed = zstd_decompress(http_response.body);
      } else if (content_encoding == "br") {
        decompressed = brotli_decompress(http_response.body);
      } else if (content_encoding == "gzip") {
        decompressed = gzip_decompress(http_response.body);
      }
      if (decompressed.has_value()) {
        http_response.body = std::move(*decompressed);
      }
    }

    return http_response;
  }

  HttpResponse SendRequestStreamUpload(
      const std::string &method, const std::string &path,
      std::function<std::string(size_t)> stream_provider, size_t content_length,
      const HeaderMap &headers, int redirect_depth = 0) {
    if (redirect_depth > max_redirects_) {
      throw std::runtime_error("Too many redirects");
    }

    if (!is_valid_header_value(method) || !is_valid_header_value(path)) {
      throw std::invalid_argument("Invalid request method or path");
    }

    EnsureConnected();
    client_.StartRequest(session_id_);

    auto transfer_sub = client_.GetTransferSubscriber();

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
      if (lk == "transfer-encoding") te_val = v;
    }
    if (te_val.find("chunked") != std::string::npos) {
      is_chunked = true;
    }

    bool has_connection = false;
    bool has_accept_encoding = false;
    for (const auto &[key, value] : headers) {
      std::string lk = key;
      std::transform(lk.begin(), lk.end(), lk.begin(), safe_tolower);
      if (lk == "connection") has_connection = true;
      if (lk == "accept-encoding") has_accept_encoding = true;
      if (lk == "transfer-encoding") continue;
      request_stream << key << ": " << value << "\r\n";
    }

    if (!has_connection) {
      request_stream << "Connection: keep-alive\r\n";
    }
    if (!has_accept_encoding) {
      request_stream << "Accept-Encoding: zstd, br, gzip\r\n";
    }

    if (is_chunked) {
      request_stream << "Transfer-Encoding: chunked\r\n";
    } else {
      request_stream << "Content-Length: " << content_length << "\r\n";
    }
    request_stream << "\r\n";

    if (!client_.Send(session_id_, request_stream.str())) {
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
      if (transfer_sub) {
        while (auto event = transfer_sub->try_receive()) {
          if (event->is_send && event->session_id == session_id_) {
            if (client_bytes_in_flight >= event->bytes_transferred) {
              client_bytes_in_flight -= event->bytes_transferred;
            } else {
              client_bytes_in_flight = 0;
            }
          }
        }
      }

      size_t max_flight = 65536;
      try {
        auto profile = client_.GetConnectionProfile(session_id_);
        max_flight = profile.max_outbound_buffer_size / 2;
      } catch (const std::exception &) {
        CleanUpConnection();
        throw std::runtime_error("Connection closed during upload");
      }
      if (max_flight < 65536) max_flight = 65536;

      if (transfer_sub && client_bytes_in_flight >= max_flight) {
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
          client_.Send(session_id_, final_chunk);
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
      if (client_.Send(session_id_, payload_to_send)) {
        if (transfer_sub) {
          client_bytes_in_flight += payload_bytes;
        }
        total_sent += chunk.size();
      } else {
        CleanUpConnection();
        throw std::runtime_error("Failed to send chunk");
      }
    }

    client_.EndRequest(session_id_);

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
              HttpClientImpl<Transport> new_client(r_host, r_port);
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
    client_.StartRequest(session_id_);

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
      request_stream << "Accept-Encoding: zstd, br, gzip\r\n";
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

    if (!client_.Send(session_id_, request_stream.str())) {
      CleanUpConnection();
      throw std::runtime_error("Failed to send request");
    }

    client_.EndRequest(session_id_);

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
              HttpClientImpl<Transport> new_client(r_host, r_port);
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
      if (!line.empty() && line.back() == '\r') line.pop_back();

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
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (line.empty()) break;

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

using HttpClient = HttpClientImpl<TcpClientTransport>;

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

  void SetMaxOutboundBufferSize(size_t size) {
    max_outbound_buffer_size_ = size;
  }
  size_t GetMaxOutboundBufferSize() const { return max_outbound_buffer_size_; }

  void SetCloseTimeout(std::chrono::milliseconds timeout) {
    close_timeout_ = timeout;
  }
  std::chrono::milliseconds GetCloseTimeout() const { return close_timeout_; }

  bool Connect(const std::string &url,
               std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
    std::string protocol, host, path;
    uint16_t port;
    if (!parse_url(url, protocol, host, port, path)) {
      internal::Log(LogSeverity::Error, "WebSocketClient",
                    "Connect failed: URL parsing failed");
      return false;
    }

    ssl_enabled_ = (protocol == "https" || protocol == "wss");

    client_ = std::make_unique<cpptcpnet::TcpClient>();
    client_->SetNoDelay(true);
    if (max_outbound_buffer_size_ > 0) {
      client_->SetMaxOutboundBufferSize(max_outbound_buffer_size_);
    }
    if (ssl_enabled_) {
      cpptcpnet::TcpClient::SslClientConfig ssl_cfg;
      ssl_cfg.verify_peer = false;
      client_->EnableSSL(ssl_cfg);
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
            if (!handshake_completed_) {
              return;
            }
            run_process_frames = true;
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
      internal::Log(LogSeverity::Error, "WebSocketClient",
                    "Connect failed: TcpClient::Connect returned 0");
      Disconnect();
      return false;
    }

    std::unique_lock<std::mutex> lock(mtx_);
    if (!cv_.wait_for(lock, timeout,
                      [this] { return connected_ || disconnected_; })) {
      internal::Log(
          LogSeverity::Error, "WebSocketClient",
          "Connect failed: Timeout waiting for TCP connection (connected_=" +
              std::to_string(connected_) +
              ", disconnected_=" + std::to_string(disconnected_) + ")");
      lock.unlock();
      Disconnect();
      return false;
    }
    if (disconnected_ || !connected_) {
      internal::Log(
          LogSeverity::Error, "WebSocketClient",
          "Connect failed: TCP disconnected or not connected (connected_=" +
              std::to_string(connected_) +
              ", disconnected_=" + std::to_string(disconnected_) + ")");
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
      internal::Log(LogSeverity::Error, "WebSocketClient",
                    "Connect failed: Failed to send handshake request");
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
      internal::Log(LogSeverity::Error, "WebSocketClient",
                    "Connect failed: Timeout waiting for handshake response. "
                    "Buffer size: " +
                        std::to_string(buffer_.size()));
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
      internal::Log(
          LogSeverity::Error, "WebSocketClient",
          "Connect failed: Response did not contain 101 Switching Protocols");
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
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (line.empty()) break;
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
      internal::Log(
          LogSeverity::Error, "WebSocketClient",
          "Connect failed: Sec-WebSocket-Accept header missing or mismatched");
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
        if (c) c->Stop();
        if (w) w->Stop();
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
      if (!connected_) return;
    }

    SendFrame(0x08, payload);

    bool should_wait = false;
    {
      std::lock_guard<std::mutex> lock(mtx_);
      if (status_code == 1000 && !processing_frames_) {
        should_wait = true;
      }
    }
    if (should_wait) {
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
    if (!connected_ || session_id_ == 0 || !client_) return false;

    std::vector<uint8_t> frame;
    frame.push_back(0x80 | (opcode & 0x0F));

    uint64_t len = payload.size();
    uint8_t len_byte = 0x80;  // client MUST mask
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
            if (on_close_) {
              on_close_(1002, "Protocol error");
            }
            Close(1002, "Protocol error");
            return;
          }
          if (is_too_big) {
            if (on_close_) {
              on_close_(1009, "Message too big");
            }
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
  size_t max_outbound_buffer_size_ = 10 * 1024 * 1024;
  std::chrono::milliseconds close_timeout_{1000};

  bool ws_fragmented_ = false;
  uint8_t ws_fragmented_opcode_ = 0;
  std::vector<uint8_t> ws_assembled_payload_;
  bool processing_frames_ = false;
};

}  // namespace cpphttp