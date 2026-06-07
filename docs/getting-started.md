---
layout: default
---

[🏠 Home](./) | [Next: Basic Usage >](./basic-usage.html)
<hr>

# Getting Started

To use `cpp-http`, include the `cpphttp.hpp` header in your project. It depends on `cpp-tcpnet` (the foundational network engine), which in turn depends on `cpp-pubsub` and `cpp-asyncworker`.

## Integration

### 1. CMake: `FetchContent` (Recommended)

You can have CMake automatically download and integrate `cpp-http` and its dependencies. 

```cmake
include(FetchContent)

FetchContent_Declare(
  cpphttp
  GIT_REPOSITORY https://github.com/jonoton/cpp-http.git
  GIT_TAG main
)
FetchContent_MakeAvailable(cpphttp)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE cpphttp::cpphttp)
```

By linking `cpphttp::cpphttp`, CMake will automatically set up the header include paths and pull in the `cpp-tcpnet` dependency as well.

### 2. Manual Compilation (Without CMake)

If you are compiling manually without CMake, you will need to add all sibling library include folders to your header search path:
* `cpp-http`
* `cpp-tcpnet`
* `cpp-pubsub`
* `cpp-asyncworker`

On Linux/macOS, you must link the `pthread` library. On Windows, socket libraries are handled automatically.

```bash
# Basic compilation command:
g++ -std=c++17 -pthread -I/path/to/cpp-http -I/path/to/cpp-tcpnet -I/path/to/cpp-pubsub -I/path/to/cpp-asyncworker main.cpp -o my_app
```

---
[🏠 Home](./) | [Next: Basic Usage >](./basic-usage.html)
