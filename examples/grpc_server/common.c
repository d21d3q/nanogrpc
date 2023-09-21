/* Simple binding of nanopb streams to TCP sockets.
 */

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pb_encode.h>
#include <pb_decode.h>

#include "common.h"

bool write_callback(pb_ostream_t *stream, const uint8_t *buf, size_t count)
{

    int fd = (intptr_t)stream->state;
    int result = send(fd, buf, count, 0);

    return result == count;
}

bool read_callback(pb_istream_t *stream, uint8_t *buf, size_t count)
{
    int fd = (intptr_t)stream->state;
    int result;
    
    result = recv(fd, buf, count, MSG_WAITALL);
    
    if (result == 0 || buf[0] == 0) {
        stream->bytes_left = 0; /* EOF */
        return false;
    }

    printf("left %lu, reading %lu:", stream->bytes_left, count);
    printf("%d\n", result);

    int i;
    for (i = 0; i < result; i++) {
        printf("%02x", buf[i]);
    }

    printf("\n");
    
    return result == count;
}

pb_ostream_t pb_ostream_from_socket(int fd)
{
    pb_ostream_t stream = {&write_callback, (void*)(intptr_t)fd, SIZE_MAX, 0};
    return stream;
}

pb_istream_t pb_istream_from_socket(int fd)
{
    pb_istream_t stream = {&read_callback, (void*)(intptr_t)fd, SIZE_MAX};
    return stream;
}
