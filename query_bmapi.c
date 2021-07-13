#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "query_bmapi.h"
#include "logger.h"

const char* bmminer_addr = "127.0.0.1";
const unsigned short port = 4028;

bmapi_err_code query_bmapi(const char* query, char* reply_buf, size_t reply_buf_size) {
    struct protoent *protoent;
    in_addr_t in_addr;
    int socket_file_descriptor;
    ssize_t nbytes_total, nbytes_last, request_len;
    struct sockaddr_in sockaddr_in;

    /* Build the socket. */
    protoent = getprotobyname("tcp");
    if (protoent == NULL) {
        app_log(LOG_ERR, "getprotobyname");
        return BMAPI_FAIL;
    }

    in_addr = inet_addr(bmminer_addr);
    if (in_addr == (in_addr_t)-1) {
        app_log(LOG_ERR, "inet_addr(\"%s\")\n", bmminer_addr);
        return BMAPI_FAIL;
    }
    sockaddr_in.sin_addr.s_addr = in_addr;
    sockaddr_in.sin_family = AF_INET;
    sockaddr_in.sin_port = htons(port);

    socket_file_descriptor = socket(AF_INET, SOCK_STREAM, protoent->p_proto);
    if (socket_file_descriptor == -1) {
        app_log(LOG_ERR, "socket");
        return BMAPI_FAIL;
    }

    /* connect. */
    if (connect(socket_file_descriptor, (struct sockaddr*)&sockaddr_in, sizeof(sockaddr_in)) == -1) {
        app_log(LOG_DEBUG, "Can't connect");
        close(socket_file_descriptor);
        return BMAPI_CANNOT_CONNECT;
    }

    /* Send request. */
    nbytes_total = 0;
    request_len = strlen(query);
    while (nbytes_total < request_len) {
        nbytes_last = write(socket_file_descriptor, query + nbytes_total, request_len - nbytes_total);
        if (nbytes_last == -1) {
            app_log(LOG_ERR, "write");
            close(socket_file_descriptor);
            return BMAPI_FAIL;
        }
        nbytes_total += nbytes_last;
    }

    /* Read the response. */
    app_log(LOG_DEBUG, "before first read\n");
    nbytes_total = 0;
    while ((nbytes_last= read(socket_file_descriptor, reply_buf + nbytes_total, reply_buf_size - nbytes_total)) > 0) {
        app_log(LOG_DEBUG, "after a read\n");
        nbytes_total += nbytes_last;
    }
    app_log(LOG_DEBUG, "after last read\n");
    if (nbytes_total == -1) {
        app_log(LOG_DEBUG, "read");
        close(socket_file_descriptor);
        return BMAPI_FAIL;
    }

    close(socket_file_descriptor);
    return BMAPI_OK;
}
