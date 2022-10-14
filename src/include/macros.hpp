#pragma once

#include <stdio.h>
#include "config.hpp"

#define MAX_HOSTS           128
#define UNIDENTIFIED_HOST   MAX_HOSTS + 1
#define LOG_MSG_SIZE        20


#define traceerror()                           \
    (fprintf(stderr, "%s:%s() at line %d\n",    \
             __FILE__, __FUNCTION__, __LINE__))

#define trace()                         \
    (printf("%s() at line %d\n", __FUNCTION__, __LINE__))

#define setuphost(host, ip, port)   \
        host.sin_family = AF_INET;  \
        host.sin_addr.s_addr = ip;  \
        host.sin_port = port;

#define SOCKET() (socket(AF_INET, SOCK_DGRAM, 0))
