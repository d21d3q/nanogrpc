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
#include "ng.h"
#include "common.h"

/* DEFINE_FILL_WITH_ZEROS_FUNCTION(RpcPacketRequest_CS)
DEFINE_FILL_WITH_ZEROS_FUNCTION(RpcPacketResponse_CS) */

/* This is only for holding methods, etc. It has to be reimplemented
for client purposes. (temporary) */
ng_grpc_handle_t hGrpc;
Chunk requestChunk_holder;
Chunk responseChunk_holder;
ng_methodContext_t context;
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
  context.request = (void *)&requestChunk_holder;
  context.response = &responseChunk_holder;
  ng_setMethodContext(&Transfer_Write_method, &context);
  ng_setMethodCallback(&Transfer_Write_method, (void *)&dummyCallback);
  /* ng_GrpcRegisterService(&hGrpc, &FileServer_service); */
  /* hGrpc.input = &istream;
  hGrpc.output = &ostream; */
}

void setupTransferStart(Chunk *chunk) {
    chunk->type = Chunk_Type_START;
    chunk->has_desired_session_id = true;
    chunk->desired_session_id = 4455;
}

bool sendFile(int fd, char *path) {
    RpcPacketRequest_CS gRequest = RpcPacketRequest_CS_init_zero;
    RpcPacketResponse_CS gResponse = RpcPacketResponse_CS_init_zero;
    bool validRequest;
    size_t requestSize;

    /* Setup streams */
    pb_ostream_t output = pb_ostream_from_socket(fd);

    /* General request setup */
    gRequest.type = PacketType_REQUEST;
    gRequest.method_id = 3165279579;
    gRequest.payload.funcs.encode = &encodeRequestCallback;
    ng_encodeMessageCallbackArgument_t arg;
    arg.method = &Transfer_Write_method;
    arg.context = &context;
    gRequest.payload.arg = &arg;

    validRequest = pb_get_encoded_size(&requestSize, RpcPacketRequest_CS_fields, &gRequest);

    if (!validRequest) {
        printf("Invalid request\n");
        return false;
    }

    setupTransferStart(context.request);

    /* Encode the request. It is written to the socket immediately
        * through our custom stream. */
    if (!pb_encode_ex(&output, RpcPacketRequest_CS_fields, &gRequest, PB_ENCODE_NULLTERMINATED))
    {
        fprintf(stderr, "Encoding failed: %s\n", PB_GET_ERROR(&output));
        return false;
    }

    pb_istream_t istream = pb_istream_from_socket(fd);

    /* Receive response */
    if (!pb_decode_ex(&istream, RpcPacketResponse_CS_fields, &gResponse, PB_ENCODE_NULLTERMINATED)) {
        printf("Error: %s\n", PB_GET_ERROR(&istream));
        return false;
    }

    if (gResponse.payload == NULL){
        fprintf(stderr, "no data\n");
        /*fprintf(stderr, "status: %d", gResponse.grpc_status);*/
        pb_release(RpcPacketResponse_CS_fields, &gResponse);
        return false;
    }

    pb_istream_t input = pb_istream_from_buffer(gResponse.payload->bytes, gResponse.payload->size);

    if (!pb_decode(&input, Chunk_fields, &responseChunk_holder))
    {
        fprintf(stderr, "Decode response failed: %s\n", PB_GET_ERROR(&input));
        pb_release(RpcPacketResponse_CS_fields, &gResponse);
        return false;
    }
    pb_release(RpcPacketResponse_CS_fields, &gResponse);

    if (responseChunk_holder.type != Chunk_Type_START_ACK) {
        fprintf(stderr, "Invalid response type\n");
        return false;
    }

    size_t currentPointer = 0;

    int i;
    for (i = 0; i < 4; i++) {
        requestChunk_holder.type = Chunk_Type_DATA;
        requestChunk_holder.offset = responseChunk_holder.offset;
        
        memcpy(requestChunk_holder.data.bytes, dummyFileBuffer + currentPointer, 4096);
        requestChunk_holder.data.size = 4096;

        pb_encode_ex(&output, RpcPacketRequest_CS_fields, &gRequest, PB_ENCODE_NULLTERMINATED);

        istream = pb_istream_from_socket(fd);

        pb_decode_ex(&istream, RpcPacketResponse_CS_fields, &gResponse, PB_ENCODE_NULLTERMINATED);
        pb_decode(&input, Chunk_fields, &responseChunk_holder);
        
        printf("offset: %llu\n", responseChunk_holder.offset);
        currentPointer = responseChunk_holder.offset;
    }


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

    /* Close connection */
    close(sockfd);

    return 0;
}
