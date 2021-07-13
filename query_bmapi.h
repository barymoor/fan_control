#pragma once
#include <stddef.h>

typedef enum  {
    BMAPI_OK,
    BMAPI_CANNOT_CONNECT,
    BMAPI_FAIL = -1
} bmapi_err_code;

bmapi_err_code query_bmapi(const char* query, char* reply_buf, size_t reply_buf_size);

