
# NanoGRPC-TS

This is a typescript client implementation of the [nanogrpc](../README.md) RPC protocol.
It is intended for use with [protobufjs](https://www.npmjs.com/package/protobufjs).


## Usage

To use the library, you create a RPC Service Factory. From there you can create services from any [protobufjs](https://www.npmjs.com/package/protobufjs) generated service.

```ts
import * as nanogrpc from "@soundboks/nanogrpc"
import { ExampleService } from "./generated/ExampleService"

const RPCTransport: nanogrpc.RPCTransportAdapter = {
    send: async (request: nanogrpc.RpcPacketRequest) => {
        /// transport implementation
        return response
    }
}

const NanoGRPC = nanogrpc.createRPCServiceFactory(service)

const ExampleService = NanoGRPC.create(ExampleService)

ExampleService.hello().then(console.log)
```


## Transports

To use nanogrpc, you must supply an appropriate transport. The transport must implement the quite simple interface

```ts
export interface RPCTransportAdapter {
    send: (request: RpcPacketRequest) => Promise<RpcPacketResponse>;
}
```

It is important however, that the transport conforms to some implicit assumptions.

### Parallel Service Calls

NanoGRPC does not restrict the capability of any amount of services to make simultaneous requests to multiple services. It is up to the transport to implement queuing mechanisms or whatever resolution strategy is appropriate. A HTTP transport might want to implement parallel requests, while a UART implementation might want to use a mutex.

### Errors

Errors thrown by the transport are propagated through the stack to the consumer. It is expected that if transport fails due to physical layer interruptions or data validity errors, the transport should throw.


### Design Rationale

The interface of the transport adapter is from unencoded RpcPacketRequests to unencoded RpcPacketResponses. This is to allow the transport layer the ability to do deep introspection, if needed to implement complex behaviours, rather than just operating on plain bytes.