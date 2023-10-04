### Acknowledgements

Many thanks to the original author of nanogrpc ([https://github.com/d21d3q/nanogrpc](https://github.com/d21d3q/nanogrpc)), which
this repository is a developed fork of.

# About
This is a general gRPC implementation based on nanoprotobuffers for use over any byte-oriented transport.  

Instead of transporting messages over the
[standard HTTP protocol](https://github.com/grpc/grpc/blob/master/doc/PROTOCOL-HTTP2.md) and using URI paths for identifying resources,
it wraps requests and responses into [protobuf messages](./nanogrpc.proto) and uses crc32 service and method hashes.

It is intended to be usable with any underlying transport and implements no framing mechanisms on it's own.

#### Intended example use cases:

- UART with [HLDC](https://en.wikipedia.org/wiki/High-Level_Data_Link_Control) framing
- BLE with [HLDC](https://en.wikipedia.org/wiki/High-Level_Data_Link_Control)
- TPC/UDP sockets, IPC pipes, Event Queues


# Usage

## gRPC Dispatcher

### Minimal Setup

```c
#include "ng_server.h"
#include "HelloService.pb.h"

ng_grpc_handle_t hGrpc;
ng_methodContext_t globalContext;

// storage for the global context
uint8_t globalRequest_holder[4200];
uint8_t globalResponse_holder[4200];

// defined elsewhere
ng_CallbackStatus_t HelloService_Commands_Hello(ng_methodContext_t *context, HelloRequest* request, HelloResponse* response);
ng_CallbackStatus_t HelloService_Commands_GoodBye(ng_methodContext_t *context, HelloRequest* request, HelloResponse* response);

int main() {
    globalContext.request = (void *)&globalRequest_holder;
    globalContext.response = (void *)&globalResponse_holder;

    HelloService_service_init(),

    ng_setMethodContext(&HelloService_Hello_method, &globalContext);
    ng_setMethodContext(&HelloService_GoodBye_method, &globalContext);

    HelloService_Hello_setMethodCallback(&HelloService_Commands_Hello);
    HelloService_GoodBye_setMethodCallback(&HelloService_Commands_GoodBye);

    np_GrpcRegisterService(&hGrpc, &HelloService_service);

    // more on this else where
    // suffice to say these need to be pb_istream and pb_ostream respectively
    *hGrpc.input = input_stream();
    *hGrpc.output = output_stream();

    for(;;) {
        ng_GrpcParseBlocking(&hGrpc);
    }
}
```
### Generating Services

`protoc --plugin=$NANOGRPC_DIR/generator/protoc-gen-grpc --grpc_out=$BUILD_DIR Service.proto`

Given an example proto file like HelloService.proto

```proto
syntax = "proto3";

service HelloService {
    rpc Hello(HelloRequest) returns (HelloResponse);
    rpc GoodBye(HelloRequest) returns (HelloResponse);
}

message HelloRequest {
    string message = 1;
}

message HelloResponse {
    string message = 1;
}
```

this will generate a HelloService.pb.h and HelloService.pb.c. To setup the service, you include and register it to the global gRPC dispatcher. A quick overview of all the generated types:

```c
// The handle to identify the service.
// This is what you use to register the service
//
// f.e. np_GrpcRegisterService(&hGrpc, &HelloService_service);
extern ng_service_t HelloService_service;

// Each method on the service will generate a strongly typed signature for your callback to implement
// These signatures always return ng_CallbackStatus_t and take the ng_methodContext_t as the first argument
// The second and third arguments are your defined message request and response types respectively
typedef ng_CallbackStatus_t (*HelloService_Hello_method_callback_t)(ng_methodContext_t *context, HelloRequest* request, HelloResponse* response);

// Each method also generates a helper for setting the method callback
// This ensures that the callback confirms to the type signature of the service method
bool HelloService_Hello_setMethodCallback(HelloService_Hello_method_callback_t callback);

// The method handle used to interact with the method and register it to the gRPC dispatcher
// 
// f.e. ng_setMethodContext(&HelloService_Hello_method, &globalContext);
extern ng_method_t HelloService_Hello_method;

// Same as above
typedef ng_CallbackStatus_t (*HelloService_GoodBye_method_callback_t)(ng_methodContext_t *context, HelloRequest* request, HelloResponse* response);
bool HelloService_GoodBye_setMethodCallback(HelloService_GoodBye_method_callback_t callback);
extern ng_method_t HelloService_GoodBye_method;

// Each services comes with an init function to setup internal state in nanogrpc
// Call this before once before interacting with the service
ng_GrpcStatus_t HelloService_service_init();
``````


### Concept of contexts, blocking and non blocking parsing
Each method can hold pointer to structure (`context`) which holds pointers to
to request and response structures and pointer next context structure. It is
needed for implementing non blocking parsing.

In blocking mode (`ng_GrpcParseBlocking`) only one (default) context is being
used in order to keep it simple (to use). Callback cannot schedule response
for later time. In such case error message will be sent.

In nonblocking parsing when server calls method there it could be possible to
have several calls of the same method ongoing (with own contexts). For now only
one method at time is implemented. When call on specific method is ongoing, when
another one arrives, previous will be removed. In order to handle several
methods of one type, there is need of managing not finished methods. It can be
achieved by introducing timeout functionality or keeping linked list of
contexts in form of queue. Newly registered would be placed in front of queue,
and removed to the end. In case overflow the latest call be overridden, but
it won't block. It could be default behavior in case of not having timeout.

When call arrives, callback can respond immediately, schedule response for
later time or do both (and inform server by return code), but will allow for
next responses only if method definition allows for server streaming.

It could also possible to dynamically allocate contexts, but since user is given
pointer to context which might be removed before he finishes call (timeout) it
will be tricky to remove it an NULL all pointers pointing to it...
To be discussed. 




## Path to methods
Paths to methods are effectively always hashed with a CRC32 in the current implementation to save bytes.
If collisions are expected, one could also enforced service hashes as well.

### Method naming
Different services may have methods with same names, but they won't be the same
methods. There are two ways to solve this problem:
* Use unique names for methods
* Use separate source file for each service, define methods as static, but
adding callbacks or manipulating method request/response holders would require
referencing them by path (as string), and it sounds like a mess.

## Generator
`nanogrpc_generator.py` is a copy of `nanopb_generator.py` with removed unused
classes and methods.

