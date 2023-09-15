/* This is a simple TCP server that listens on port 1234 and provides lists
 * of files to clients, using a protocol defined in file_server.proto.
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
#include <netinet/in.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include <pb_encode.h>
#include <pb_decode.h>

#include "ng_server.h"
#include "fileproto.pb.h"
#include "common.h"

#define MAX_CHUNK_SIZE 4096
#define MIN_DELAY_MICROSECONDS 10000

ng_grpc_handle_t hGrpc;

uint8_t globalRequest_holder[4200];
uint8_t globalResponse_holder[4200];

pb_istream_t istream;
pb_ostream_t ostream;
ng_methodContext_t context;

uint32_t currentSessionId = 0;
uint32_t currentTransferringResource = 0;

uint8_t chunkBuffer[MAX_CHUNK_SIZE];
uint8_t fileBuffer[MAX_CHUNK_SIZE * 4];


bool Transfer_Write_data_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
  if (stream->bytes_left > MAX_CHUNK_SIZE) {
    return false;
  }

  pb_read(stream, chunkBuffer, stream->bytes_left);
  return true;
}

ng_CallbackStatus_t Transfer_Write_methodCallback(ng_methodContext_t *context)
{
  Chunk *request = (Chunk *)context->request;
  Chunk *response = (Chunk *)context->response;

  if (request->type == Chunk_Type_START) {

    /* If the client is requesting a new session, but haven't specified a
     * session ID, we should fail.
     */
    if (!request->has_desired_session_id) {
      return CallbackStatus_Failed;
    }

    response->type = Chunk_Type_START_ACK;
    response->has_session_id = true;
    response->session_id = request->desired_session_id;
    response->offset = 0;
    response->window_end_offset = MAX_CHUNK_SIZE;
    response->has_max_chunk_size_bytes = true;
    response->max_chunk_size_bytes = MAX_CHUNK_SIZE;
    response->has_min_delay_microseconds = true;
    response->min_delay_microseconds = MIN_DELAY_MICROSECONDS;

    currentSessionId = response->session_id;
  } else if (request->type == Chunk_Type_DATA) {

    if (request->session_id != currentSessionId) {
      return CallbackStatus_Failed;
    }

    /* Copy data from the chunk buffer to the file buffer. */
    printf("offset: %llu size: %d", request->offset, request->data.size);
    memcpy(fileBuffer + request->offset, request->data.bytes, request->data.size);

    response->type = Chunk_Type_PARAMETERS_CONTINUE;
    response->has_session_id = true;
    response->session_id = request->session_id;
    response->offset = request->offset + MAX_CHUNK_SIZE;
    response->window_end_offset = response->offset + MAX_CHUNK_SIZE;
  }

  return CallbackStatus_Ok;
};

void myGrpcInit()
{
  Transfer_service_init();
  /* ng_setMethodHandler(&SayHello_method, &Greeter_methodHandler);*/
  context.request = (void *)&globalRequest_holder;
  context.response = (void *)&globalResponse_holder;
  ng_setMethodContext(&Transfer_Write_method, &context);
  ng_setMethodCallback(&Transfer_Write_method, (void *)&Transfer_Write_methodCallback);
  ng_GrpcRegisterService(&hGrpc, &Transfer_service);
  hGrpc.input = &istream;
  hGrpc.output = &ostream;
}

/* Handle one arriving client connection.
 * Clients are expected to send a ListFilesRequest, terminated by a '0'.
 * Server will respond with a ListFilesResponse message.
 */
void handle_connection(int connfd)
{
  *hGrpc.input = pb_istream_from_socket(connfd);
  *hGrpc.output = pb_ostream_from_socket(connfd);
  ng_GrpcParseBlocking(&hGrpc);
}

int main(int argc, char **argv)
{
  int listenfd, connfd;
  struct sockaddr_in servaddr;
  int reuse = 1;
  myGrpcInit();

  /* Listen on localhost:1234 for TCP connections */
  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  servaddr.sin_port = htons(1234);
  if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
  {
    perror("bind");
    return 1;
  }
  if (listen(listenfd, 5) != 0)
  {
    perror("listen");
    return 1;
  }

  for (;;)
  {
    /* Wait for a client */
    connfd = accept(listenfd, NULL, NULL);

    if (connfd < 0)
    {
      perror("accept");
      return 1;
    }

    printf("Got connection.\n");

    while(connfd > 0) {
      handle_connection(connfd);
    }

    printf("Closing connection.\n");

    close(connfd);
  }

  return 0;
}
