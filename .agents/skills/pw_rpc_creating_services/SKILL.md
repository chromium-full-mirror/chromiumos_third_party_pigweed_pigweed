---
name: C++ Pigweed RPC Services
description: Guide to defining, implementing, and testing a new Pigweed RPC service in C++ using Nanopb, pw_protobuf, or raw methods.
---

# How to Create a New RPC Service in C++ using pw_rpc

Use this guide to create and register a new RPC service in C++ using Pigweed's `pw_rpc` module.

## Overview

To create a new RPC service, follow these four main steps:

1. **Define your service** in a `.proto` file.
2. **Configure your build system** to generate C++ code.
3. **Implement your service class** in C++.
4. **Register your service** with an RPC server.

---

## 1. Define the Service (.proto)

Create a protocol buffer file to define your service and message types. You should use `proto3` syntax.

```protobuf
// my_service.proto
syntax = "proto3";

package my_project;

import "pw_protobuf_protos/common.proto";

service MyService {
  // A simple unary RPC
  rpc GetStatus(StatusRequest) returns (StatusResponse);

  // A server streaming RPC
  rpc StreamData(StreamRequest) returns (stream StreamResponse);
}

message StatusRequest {
  string query = 1;
}

message StatusResponse {
  bool ok = 1;
  string message = 2;
}

message StreamRequest {
  uint32 count = 1;
}

message StreamResponse {
  uint32 index = 1;
  float value = 2;
}
```

---

## 2. Configure the Build System

Pigweed supports multiple protobuf backends. You should decide which backend to use based on the following priority:

1. Use the library the user asks for.
2. Use the default library used by other RPC services in the project (search for it in build files).
3. Default to Nanopb if the project has it installed.
4. Otherwise, ask the user whether to use Nanopb or pwpb.

### Bazel Configuration

Use `nanopb_rpc_proto_library` or `pwpb_rpc_proto_library` to generate RPC headers.

```python
# BUILD.bazel
load(
    "@pigweed//pw_protobuf_compiler:pw_proto_library.bzl",
    "nanopb_proto_library",
    "nanopb_rpc_proto_library",
)

proto_library(
    name = "my_service_proto",
    srcs = ["my_service.proto"],
    deps = [
        "@pigweed//pw_protobuf:common_proto",
    ],
)

nanopb_proto_library(
    name = "my_service_nanopb",
    deps = [":my_service_proto"],
)

nanopb_rpc_proto_library(
    name = "my_service_nanopb_rpc",
    nanopb_proto_library_deps = [":my_service_nanopb"],
    deps = [":my_service_proto"],
)
```

### GN

In a `BUILD.gn` file, use the `pw_proto_library` template. It automatically generates subtargets for all backends (e.g., `.pwpb_rpc`, `.nanopb_rpc`, `.raw_rpc`).

```gn
import("$dir_pw_protobuf_compiler/proto.gni")

pw_proto_library("my_protos") {
  sources = [ "my_service.proto" ]
}

pw_source_set("my_service_impl") {
  sources = [ "my_service.cc" ]
  deps = [
    ":my_protos.nanopb_rpc", # For message-based types
    # or :my_protos.pwpb_rpc
    # or :my_protos.raw_rpc
  ]
}
```

### CMake

In a `CMakeLists.txt` file, use the `pw_proto_library` function. It also generates subtargets for all backends.

```cmake
include($ENV{PW_ROOT}/pw_build/pigweed.cmake)
include($ENV{PW_ROOT}/pw_protobuf_compiler/proto.cmake)

pw_proto_library(my_protos
  SOURCES
    my_service.proto
)

add_library(my_service_impl ...)
target_link_libraries(my_service_impl PUBLIC
  my_protos.nanopb_rpc
)
```

> [!TIP]
> **Run the build now!** You should run the build immediately after configuring it to generate the RPC headers. This allows you to open the generated header and copy the **implementation stubs** from the bottom of the file, saving you from writing the method signatures manually.
>
> - **Bazel:** `bazelisk build //my_project:my_service_nanopb_rpc`
> - **GN:** `ninja -C out`
> - **CMake:** `cmake --build out`

---

## Where to Find Generated Headers

The generated headers are placed in your build output directory. Their exact location varies by your build system and toolchain, but your **C++ include path will always match your source declaration** in the `proto_library`.

- **Include Path:** If your proto is at `my_project/my_service.proto`, your include will be `"my_project/my_service.rpc.pb.h"` (for Nanopb).
- **Header Extensions:** Depending on the protobuf backend used:
  - Nanopb: `.rpc.pb.h`
  - pw_protobuf: `.rpc.pwpb.h`
  - Raw: `.raw_rpc.pb.h`
- **Physical Location (Bazel):** Usually inside `bazel-bin/` or `bazel-out/`. You can find them with precise extensions:
  ```bash
  find bazel-out -name "*.rpc.pb.h" -o -name "*.rpc.pwpb.h" -o -name "*.raw_rpc.pb.h"
  ```
- **Physical Location (GN):** Usually inside `out/<toolchain>/gen/` (e.g. `out/protocol_buffer/gen/`). You can find them with precise extensions:
  ```bash
  find out/ -name "*.rpc.pb.h" -o -name "*.rpc.pwpb.h" -o -name "*.raw_rpc.pb.h"
  ```

---

## 3. Implement the Service Class in C++

Inherit from your generated service base class. Your generated header will be named `[proto_file].rpc.pb.h` for Nanopb or `[proto_file].rpc.pwpb.h` for `pw_protobuf`.

> [!TIP]
> The generated RPC headers include **implementation stubs** at the bottom of the file. You can copy-paste these stubs to get started with implementing your service.

### Using Nanopb (Recommended)

Include the generated header `"my_service.rpc.pb.h"`.

```cpp
#include "my_service.rpc.pb.h"
#include "pw_status/status.h"

class MyServiceImpl final : public my_project::pw_rpc::nanopb::MyService::Service<MyServiceImpl> {
 public:
  // 1. Unary RPC
  // 1a. Synchronous Unary RPC
  pw::Status GetStatus(const my_project_StatusRequest& request,
                       my_project_StatusResponse& response) {
    response.ok = true;
    return pw::OkStatus();
  }

  // 1b. Asynchronous Unary RPC (Alternative)
  void GetStatus(const my_project_StatusRequest& request,
                 pw::rpc::NanopbUnaryResponder<my_project_StatusResponse>& new_responder) {
    // You MUST move the responder to a class member or persistent context,
    // otherwise it will close when the function returns.
    async_responder_ = std::move(new_responder);
  }

  // Later from another thread, timer, or event loop:
  void CompleteAsyncGetStatus() {
    if (async_responder_.active()) {
      my_project_StatusResponse response{.ok = true};
      async_responder_.Finish(response, pw::OkStatus());
    }
  }

  // 2. Server Streaming RPC
  void StreamData(const my_project_StreamRequest& request,
                  pw::rpc::ServerWriter<my_project_StreamResponse>& writer) {
    for (uint32_t i = 0; i < request.count; ++i) {
      my_project_StreamResponse response;
      response.index = i;
      response.value = 42.0f;
      if (!writer.Write(response).ok()) {
        break; // Stop if write fails (e.g., channel closed)
      }
    }
    // Finish the stream synchronously. To stream asychronously, move the writer
    // into a class member.
    writer.Finish(pw::OkStatus());
  }

  // 3. Client Streaming RPC
  void SendData(pw::rpc::ServerReader<my_project_StreamRequest, my_project_StatusResponse>& new_reader) {
    // You MUST move the reader to a class member or persistent context,
    // otherwise it will close when the function returns.
    client_reader_ = std::move(new_reader);

    client_reader_.set_on_next([this](const my_project_StreamRequest& req) {
      // Process incoming request `req` from client
    });
  }

  // 4. Bidirectional Streaming RPC
  void Chat(pw::rpc::ServerReaderWriter<my_project_StatusRequest, my_project_StatusResponse>& new_reader_writer) {
    bidi_stream_ = std::move(new_reader_writer);

    bidi_stream_.set_on_next([this](const my_project_StatusRequest& req) {
      // Example: Echoing back a response for each request
      my_project_StatusResponse resp{.ok = true};
      bidi_stream_.Write(resp);
    });
  }

  // Later, close the streams when done...
  void CloseStreams() {
    if (client_reader_.active()) {
      client_reader_.Finish({.ok = true}, pw::OkStatus());
    }
    if (bidi_stream_.active()) {
      bidi_stream_.Finish(pw::OkStatus());
    }
  }

 private:
  pw::rpc::ServerReader<my_project_StreamRequest, my_project_StatusResponse> client_reader_;
  pw::rpc::ServerReaderWriter<my_project_StatusRequest, my_project_StatusResponse> bidi_stream_;
  pw::rpc::NanopbUnaryResponder<my_project_StatusResponse> async_responder_;
};
```

> [!IMPORTANT]
> Make sure to use `std::move` if passing the `ServerWriter` to another thread or context to avoid accidentally closing it when it goes out of scope.

### Using pw_protobuf (Alternative)

Include the generated header `"my_service.rpc.pwpb.h"`. Note that you will use `::Message` for generated types.

```cpp
#include "my_service.rpc.pwpb.h"

class MyServicePwpbImpl final : public my_project::pw_rpc::pwpb::MyService::Service<MyServicePwpbImpl> {
 public:
  // 1a. Synchronous Unary RPC
  pw::Status GetStatus(const my_project::StatusRequest::Message& request,
                       my_project::StatusResponse::Message& response) {
    response.ok = true;
    return pw::OkStatus();
  }

  // 1b. Asynchronous Unary RPC (Alternative)
  void GetStatus(const my_project::StatusRequest::Message& request,
                 pw::rpc::PwpbUnaryResponder<my_project::StatusResponse::Message>& new_responder) {
    // You MUST move the responder to a class member or persistent context.
    async_pwpb_responder_ = std::move(new_responder);
  }

  // Later from another thread, timer, or event loop:
  void CompleteAsyncPwpbGetStatus() {
    if (async_pwpb_responder_.active()) {
      my_project::StatusResponse::Message response{.ok = true};
      async_pwpb_responder_.Finish(response, pw::OkStatus());
    }
  }

  void StreamData(const my_project::StreamRequest::Message& request,
                  pw::rpc::ServerWriter<my_project::StreamResponse::Message>& writer) {
    // Implementation...
  }

 private:
  pw::rpc::PwpbUnaryResponder<my_project::StatusResponse::Message> async_pwpb_responder_;
};
```

### Falling Back to Raw Methods (Advanced)

You should use Raw RPC when you hit limitations of higher-level libraries or want to maximize performance. Your common use cases include:

1. **Callback Limitations (Nanopb & `pw_protobuf::Message`):** When a message contains fields requiring callbacks (e.g., repeated strings or submessages of unknown size), both Nanopb and `pw_protobuf`'s high-level `Message` API require you to set the callback function _before_ decoding. Since standard `pw_rpc` internally decodes the message _before_ it calls your method, you never get a chance to set it. Falling back to Raw RPC gives you the raw bytes so you can set your callbacks and decode manually.
2. **Zero-Allocation Receiving (Sparse Parsing):** Standard generated RPC methods force you to use a static object representation of the message. If you have a complex message but only care about reading a few fields, this object can take up unnecessary RAM. Raw RPC lets you use generic `pw::protobuf::StreamDecoder` to scan the raw byte stream field-by-field without instantiating any object tree view in memory.
3. **Zero-Copy Responses (In-Place Wire Encoding):** Raw RPC provides mechanisms like `FinishCallback()` which give you direct access to the actual `ByteSpan` of the `pw_rpc` wire buffer. You can use `pw::protobuf::StreamEncoder` to write your response fields **directly into the wire buffer** in-place, bypassing any intermediate scratch buffers or C structures entirely for maximum performance.
4. **Low-Overhead Benchmarking (e.g., a simple loop echo):** When you want to minimize processing overhead to stress-test or benchmark the transport layer itself. Pigweed's own `BenchmarkService` uses Raw RPC for lowest-latency echo.

You can mix Raw methods inside a Nanopb or `pw_protobuf` service! If a method signature uses standard types like `pw::ConstByteSpan` and `pw::rpc::RawUnaryResponder`, `RawServerWriter`, etc., Pigweed will automatically recognize it as a Raw method and bypass decoding.

```cpp
class MyMixedService final : public my_project::pw_rpc::nanopb::MyService::Service<MyMixedService> {
 public:
  // Using Nanopb (Standard)
  pw::Status GetStatus(const my_project_StatusRequest& request,
                       my_project_StatusResponse& response) {
    return pw::OkStatus();
  }

  // Falling back to Raw (Advanced) for a specific method
  void StreamData(pw::ConstByteSpan request, pw::rpc::RawServerWriter& writer) {
    // Manually parse request or write raw bytes to writer
  }
};
```

---

## 4. Register the Service with the Server

Instantiate your service implementation and register it with your `pw::rpc::Server`.

```cpp
#include "pw_rpc/server.h"

// Define channels and server (usually done by the system)
constexpr pw::rpc::Channel kChannels[] = { /* ... */ };
pw::rpc::Server server(kChannels);

MyServiceImpl my_service;

void InitApp() {
  server.RegisterService(my_service);
}
```

If you are using `pw_system`, you can register it in `UserAppInit`:

```cpp
#include "pw_system/rpc_server.h"

namespace pw::system {

void UserAppInit() {
  pw::system::GetRpcServer().RegisterService(my_service);
}

}  // namespace pw::system
```

---

## 5. Unit Test the Service

You can unit test `pw_rpc` services using Pigweed's test invocation contexts. These contexts manage the lifecycle of the RPC call, capture response packets, and allow you to simulate client-side streaming events without running a full server.

Depending on your backend, you will use:
-   `PW_NANOPB_TEST_METHOD_CONTEXT`
-   `PW_PWPB_TEST_METHOD_CONTEXT`
-   `PW_RAW_TEST_METHOD_CONTEXT`

### Unary test example

```cpp
#include "pw_rpc/nanopb/test_method_context.h"
#include "my_service.rpc.pb.h"

TEST(MyServiceTest, GetStatus_Success) {
  // 1. Declare context
  PW_NANOPB_TEST_METHOD_CONTEXT(MyServiceImpl, GetStatus) context;

  // 2. Call RPC
  my_project_StatusRequest request{.query = "test"};
  EXPECT_EQ(pw::OkStatus(), context.call(request));

  // 3. Inspect results
  EXPECT_TRUE(context.response().ok);
}
```

### Server Streaming test example

```cpp
#include "pw_rpc/nanopb/test_method_context.h"

TEST(MyServiceTest, StreamData_SendsMultiplepackets) {
  PW_NANOPB_TEST_METHOD_CONTEXT(MyServiceImpl, StreamData) context;

  my_project_StreamRequest request{.count = 3};
  context.call(request);

  EXPECT_TRUE(context.done());
  EXPECT_EQ(pw::OkStatus(), context.status());

  // Verify all captured responses
  ASSERT_EQ(3u, context.responses().size());
  EXPECT_EQ(0u, context.responses()[0].index);
}
```

### Client Streaming test example

```cpp
#include "pw_rpc/nanopb/test_method_context.h"

TEST(MyServiceTest, SendData_AggregatesResults) {
  PW_NANOPB_TEST_METHOD_CONTEXT(MyServiceImpl, SendData) context;

  context.call();

  context.SendClientStream({.value = 10});
  context.SendClientStream({.value = 20});
  context.SendClientStreamEnd();

  EXPECT_TRUE(context.done());
  EXPECT_EQ(30, context.response().total);
}
```

### Bidirectional Streaming test example

```cpp
#include "pw_rpc/nanopb/test_method_context.h"

TEST(MyServiceTest, Chat_EchoesResponses) {
  PW_NANOPB_TEST_METHOD_CONTEXT(MyServiceImpl, Chat) context;

  context.call();

  context.SendClientStream({.message = "Ping"});
  ASSERT_EQ(1u, context.responses().size());
  EXPECT_STREQ("Ping", context.responses()[0].message);

  context.SendClientStream({.message = "Pong"});
  ASSERT_EQ(2u, context.responses().size());
  EXPECT_STREQ("Pong", context.responses()[1].message);

  context.SendClientStreamEnd();
}
```

### Complex Callback decoding in tests

When your service response contains callbacks (like repeated fields), standard response decoding will fail. You can pass a custom response object with parsing callbacks preloaded to decode into it in-place.

```cpp
TEST(MyServiceTest, GetList_HandlesCallbacks) {
  PW_NANOPB_TEST_METHOD_CONTEXT(MyServiceImpl, GetList) context;
  context.call({});

  MyListResponse response{};
  response.items.funcs.decode = +[](pb_istream_t* stream, const pb_field_iter_t*, void** arg) -> bool {
    // Custom decode logic...
    return true;
  };

  context.response(response); // Decodes into your object with callbacks
}
```
