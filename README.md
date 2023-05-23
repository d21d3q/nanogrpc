# Project status
At this moment project is abandoned. As written before, I had vision to make it equivalent
of [Firmata](https://www.arduino.cc/en/Reference/Firmata) but on steroids, to be able to talk
to embedded devices in very elastic way. So that one could interface embedded device
in complex way over serial interface without writing system driver (which could be neded
for advanced tasks). Think about being able to control multiple sensors on single sensor bus,
where you can configure ADC externally, make ADC readout as RPC, or even start streatming
response as continous series of readouts.  
So that synchronous server is kind of implemented as POC, but question is what would be use
case for client code, would it be enough to make client version as nanogrpc and keep it 
only for communication between embedded devices? Maybe write some gateway code so that 
you could talk to embedded device from "original" grpc?  
If anyone is interested it this library, [tll me](https://github.com/d21d3q/nanogrpc/issues/3)
at would be your use case, why and where woul ou need it, 
I think that this library has some potential and maybe one day I'll return to it.  
Or if you want take over the project feel free to contact me. 

---

# About
this is grpc implementaiton which aims communication over serial interfaces
like UART, RS232, RS485.
Think about it as equivalent of
[Firmata](https://www.arduino.cc/en/Reference/Firmata).

Instead of
[standard HTTP protocol](https://github.com/grpc/grpc/blob/master/doc/PROTOCOL-HTTP2.md)
it wraps request and response into `protobuf` messages which definition can be
found in `nanogrpc.proto` file.

This implementation is not going to provide transport layer. User needs to
implement it separately and provide `istream` and `ostream` to `ng_GrpcParse`
function which decodes `GrpcRequest`, looks for specific method, decodes it
with call callback (specified by user), and encodes everything back to ostream.

For now I am encoding data into frames base64 encoded wrapped in `>` `<`
```
>TGlrZSB0aGlz<
```
But it can be eaistly integrated for example with
[HLDC](https://en.wikipedia.org/wiki/High-Level_Data_Link_Control).
Synchronous usage can be implemented over 
[modbus](https://en.wikipedia.org/wiki/Modbus) using other command (0x43)

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

## Examples

For now I am testing it on STM32F4DISCOVERY board as submodule so that I didn't
provide any examples yet. But I will do it as soon as I will have generator
working.

## What is done so far, how to play with it
##### Concept of call IDs
When calling method, client needs to provide random/unique id, that will
identify call. This allows to handle multuple methods as one time. In normal
grpc it is being done with http2 connections, but in order not to implement
whole stack and keep stuff simple (over single serial connection) call are
being identified with IDs.

##### Concept of contexts, blocking and non blocking parsing
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

## Roadmap
* organize tests for new project structure
* switch to latest nanpb version
* add more examples
* add client c implemntation
* add Arduino examples
* some client side implementations in python, c++, and node-red or
universal gateway
* gateway

### about nanogrpc.proto
Inside of this file you can notice that messages are duplicated with `_CS`
postfix (for client side code). Those messages differ in options. On server side
data from request needs to be stored in under pointer because during time of
receiving we don't know what method is that (we could decode it immediately if
we knew method id or path in advance, grpc allows to change of order of tags),
but we can encode response in callback. On client side situation is opposite.
There are two sollutions - having duplicated messages or specifying options
during compilation time (which sounds problematic)


### other disorganized thoughts

When encoding `GrpcResponse` we could use callback to decode method request
on the fly, but we would have to be sure, that field with path have been decoded
previously. Protocol buffers specification allows to change order of encoding
encoding fields, so that decoding method request requires static buffer or
dynamic allocation.

Encoding whole `GrpcResponse` directly to `handle->ouptut` using callback for
encoding method response leads to unwanted behavior: While encoding to non
buffer stream, when during encoding (in callback) error occurs, some bytes
are alresdy written to stream, so that where is no option to undo it and change
status code. It implies to using separate buffer for storing encoded
method response.
On the other, before assigning callback to `GrpcResponse` one can calculate
size of method response to know it it there are no problem in encoding.
