/* This is a simple TCP client that connects to port 1234 and prints a list
 * of files in a given directory.
 *
 * It directly deserializes and serializes messages from network, minimizing
 * memory use.
 *
 * For flexibility, this example is implemented using posix api.
 * In a real embedded system you would typically use some other kind of
 * a communication and filesystem layer.
 */

#undef PB_ENABLE_MALLOC

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/random.h>
#include <netinet/in.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include <pb_encode.h>
#include <pb_decode.h>
#include <pb.h>

#include "fileproto.pb.h"
#include "commands.pb.h"
#include "ng.h"
#include "common.h"

/* DEFINE_FILL_WITH_ZEROS_FUNCTION(RpcPacketRequest_CS)
DEFINE_FILL_WITH_ZEROS_FUNCTION(RpcPacketResponse_CS) */

/* This is only for holding methods, etc. It has to be reimplemented
for client purposes. (temporary) */
ng_grpc_handle_t hGrpc;

uint8_t globalRequest_holder[4200];
uint8_t globalResponse_holder[4200];

ng_methodContext_t globalContext;
/* pb_istream_t istream;
pb_ostream_t ostream; */

uint8_t dummyFileBuffer[4096 * 4] = {0x55};

bool encodeRequestCallback(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
  ng_encodeMessageCallbackArgument_t *argument = (ng_encodeMessageCallbackArgument_t*)*arg;
  
  /* char *str = get_string_from_somewhere(); */
  if (!pb_encode_tag_for_field(stream, field))
      return false;
  /* we are encoding tag for bytes, but writeing submessage, */
  /* as long it is prepended with size same as bytes */
  /*return pb_encode_submessage(stream, method->response_fields, method->response_holder);*/
  return pb_encode_submessage(stream, argument->method->request_fields, argument->context->request);
}


void dummyCallback(void * a, void *b){

}

void myGrpcInit(){
  /* FileServer_service_init(); */
  /* ng_setMethodHandler(&SayHello_method, &Greeter_methodHandler);*/
  globalContext.request = (void *)&globalRequest_holder;
  globalContext.response = (void *)&globalResponse_holder;

  ng_setMethodContext(&Transfer_Write_method, &globalContext);
  ng_setMethodContext(&Commands_SayHello_method, &globalContext);

  ng_setMethodCallback(&Transfer_Write_method, (void *)&dummyCallback);
  ng_setMethodCallback(&Commands_SayHello_method, (void *)&dummyCallback);
  /* ng_GrpcRegisterService(&hGrpc, &FileServer_service); */
  /* hGrpc.input = &istream;
  hGrpc.output = &ostream; */
}

void setupTransferStart(Chunk *chunk) {
    chunk->type = Chunk_Type_START;
    chunk->has_desired_session_id = true;
    chunk->desired_session_id = 4455;
}

void setupTransferComplete(Chunk *chunk, uint32_t session_id) {
    chunk->type = Chunk_Type_COMPLETION;
    chunk->session_id = session_id;
    chunk->has_session_id = true;
    chunk->remaining_bytes = 0;
    chunk->data.size = 0;
}

bool setupRequestResponse(RpcPacketRequest_CS *request, ng_encodeMessageCallbackArgument_t *arg, ng_method_t *method) 
{
    /* General request setup */
    request->type = PacketType_REQUEST;
    request->method_id = method->method_hash;
    request->payload.funcs.encode = &encodeRequestCallback;
    arg->method = method;
    arg->context = &globalContext;
    request->payload.arg = arg;

    return true;
}

bool sendFile(int fd, char *path) {
    RpcPacketRequest_CS fileRequest = RpcPacketRequest_CS_init_zero;
    RpcPacketResponse_CS fileResponse = RpcPacketResponse_CS_init_zero;
    Chunk *requestChunk_holder = (Chunk * )globalContext.request;
    Chunk *responseChunk_holder = (Chunk * )globalContext.response;
    ng_encodeMessageCallbackArgument_t arg;
    bool validRequest;
    size_t requestSize;

    /* Setup streams */
    pb_ostream_t output = pb_ostream_from_socket(fd);

    /* General request setup */
    setupRequestResponse(&fileRequest, &arg, &Transfer_Write_method);

    validRequest = pb_get_encoded_size(&requestSize, RpcPacketRequest_CS_fields, &fileRequest);

    if (!validRequest) {
        printf("Invalid request\n");
        return false;
    }

    setupTransferStart(globalContext.request);

    /* Encode the request. It is written to the socket immediately
        * through our custom stream. */
    if (!pb_encode_ex(&output, RpcPacketRequest_CS_fields, &fileRequest, PB_ENCODE_NULLTERMINATED))
    {
        fprintf(stderr, "Encoding failed: %s\n", PB_GET_ERROR(&output));
        return false;
    }

    pb_istream_t istream = pb_istream_from_socket(fd);

    /* Receive response */
    if (!pb_decode_ex(&istream, RpcPacketResponse_CS_fields, &fileResponse, PB_ENCODE_NULLTERMINATED)) {
        printf("Error: %s\n", PB_GET_ERROR(&istream));
        return false;
    }

    if (fileResponse.payload == NULL){
        fprintf(stderr, "no data\n");
        /*fprintf(stderr, "status: %d", fileResponse.grpc_status);*/
        pb_release(RpcPacketResponse_CS_fields, &fileResponse);
        return false;
    }

    pb_istream_t input = pb_istream_from_buffer(fileResponse.payload->bytes, fileResponse.payload->size);

    if (!pb_decode(&input, Chunk_fields, responseChunk_holder))
    {
        fprintf(stderr, "Decode response failed: %s\n", PB_GET_ERROR(&input));
        pb_release(RpcPacketResponse_CS_fields, &fileResponse);
        return false;
    }

    if (responseChunk_holder->type != Chunk_Type_START_ACK) {
        fprintf(stderr, "Invalid response type\n");
        return false;
    }

    size_t currentPointer = 0;

    int i;
    for (i = 0; i < 4; i++) {
        requestChunk_holder->type = Chunk_Type_DATA;
        requestChunk_holder->offset = responseChunk_holder->offset;
        
        memset(requestChunk_holder->data.bytes, 0xAA + currentPointer / 4096, 4096);
        requestChunk_holder->data.size = 4096;
        requestChunk_holder->has_session_id = true;
        requestChunk_holder->session_id = responseChunk_holder->session_id;

        pb_encode_ex(&output, RpcPacketRequest_CS_fields, &fileRequest, PB_ENCODE_NULLTERMINATED);

        istream.bytes_left = SIZE_MAX;

        bool didDecodePackage = pb_decode_ex(&istream, RpcPacketResponse_CS_fields, &fileResponse, PB_ENCODE_NULLTERMINATED);

        input = pb_istream_from_buffer(fileResponse.payload->bytes, fileResponse.payload->size);

        bool didDecodePayload = pb_decode(&input, Chunk_fields, responseChunk_holder);

        printf("didDecodePackage: %d didDecodePayload: %d\n", didDecodePackage, didDecodePayload);
        
        printf("offset: %llu\n", responseChunk_holder->offset);
        currentPointer = responseChunk_holder->offset;
    }

    setupTransferComplete(requestChunk_holder, responseChunk_holder->session_id);

    /* Encode the completion request. It is written to the socket immediately
    * through our custom stream. */
    if (!pb_encode_ex(&output, RpcPacketRequest_CS_fields, &fileRequest, PB_ENCODE_NULLTERMINATED))
    {
        fprintf(stderr, "Encoding failed: %s\n", PB_GET_ERROR(&output));
        return false;
    }

    /* Receive response to completion request */
    istream.bytes_left = SIZE_MAX;
    bool didDecodePackage = pb_decode_ex(&istream, RpcPacketResponse_CS_fields, &fileResponse, PB_ENCODE_NULLTERMINATED);
    input = pb_istream_from_buffer(fileResponse.payload->bytes, fileResponse.payload->size);
    bool didDecodePayload = pb_decode(&input, Chunk_fields, responseChunk_holder);

    printf("didDecodePackage: %d didDecodePayload: %d\n", didDecodePackage, didDecodePayload);

    if (responseChunk_holder->type != Chunk_Type_COMPLETION_ACK) {
        fprintf(stderr, "Invalid response type\n");
        return false;
    }

    return true;
}

bool sayHello(int fd) {
    RpcPacketRequest_CS rpcRequestPackage = RpcPacketRequest_CS_init_zero;
    RpcPacketResponse_CS rpcResponsePackage = RpcPacketResponse_CS_init_zero;
    ng_encodeMessageCallbackArgument_t arg;

    /* Setup streams */
    pb_ostream_t output = pb_ostream_from_socket(fd);

    setupRequestResponse(&rpcRequestPackage, &arg, &Commands_SayHello_method);

    HelloRequest *helloRequest = (HelloRequest *)globalContext.request;
    HelloResponse *helloResponse = (HelloResponse *)globalContext.response;

    char name[] = "John";

    memcpy(helloRequest->name, name, strlen(name));

    /* Encode the request. It is written to the socket immediately
        * through our custom stream. */
    if (!pb_encode_ex(&output, RpcPacketRequest_CS_fields, &rpcRequestPackage, PB_ENCODE_NULLTERMINATED))
    {
        fprintf(stderr, "Encoding failed: %s\n", PB_GET_ERROR(&output));
        return false;
    }

    pb_istream_t input = pb_istream_from_socket(fd);

    /* Receive response */

    if (!pb_decode_ex(&input, RpcPacketResponse_CS_fields, &rpcResponsePackage, PB_ENCODE_NULLTERMINATED)) {
        printf("Error: %s\n", PB_GET_ERROR(&input));
        return false;
    }

    if (rpcResponsePackage.payload == NULL){
        fprintf(stderr, "no data\n");
        /*fprintf(stderr, "status: %d", fileResponse.grpc_status);*/
        pb_release(RpcPacketResponse_CS_fields, &rpcResponsePackage);
        return false;
    }

    pb_istream_t responseInput = pb_istream_from_buffer(rpcResponsePackage.payload->bytes, rpcResponsePackage.payload->size);

    if (!pb_decode(&responseInput, HelloResponse_fields, helloResponse))
    {
        fprintf(stderr, "Decode response failed: %s\n", PB_GET_ERROR(&responseInput));
        pb_release(RpcPacketResponse_CS_fields, &rpcResponsePackage);
        return false;
    }

    printf("response: %s\n", helloResponse->message);

    return true;
}

int main(int argc, char **argv)
{
    int sockfd;
    struct sockaddr_in servaddr;
    char *path = NULL;

    if (argc > 1)
        path = argv[1];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    /* Connect to server running on localhost:1234 */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    servaddr.sin_port = htons(1234);

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
    {
        perror("connect");
        return 1;
    }
    myGrpcInit();

    memset(dummyFileBuffer, 'a', 4096 * 4);

    if (!sendFile(sockfd, path))
        return 2;

    sayHello(sockfd);

    /* Close connection */
    close(sockfd);

    return 0;
}
