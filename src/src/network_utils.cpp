#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <stdio.h>
#include <unistd.h>

#include "network_utils.hpp"

int get_socket(uint32_t buffer_size)
{
    int sockfd, err;
    uint size_of_socket_buf_size = sizeof(buffer_size);

    int check;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1)
    {
        perror("socket");
        return -1;
    }

    // getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<void*>(&check), &size_of_socket_buf_size);
    // printf("Original socket buffer size: %d\n", check);

    err = setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF,
        reinterpret_cast<void*>(&buffer_size), size_of_socket_buf_size);
    if (err == -1)
    {
        perror("setsockopt");
        close(sockfd);
        return -1;
    }
    // getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<void*>(&check), &size_of_socket_buf_size);
    // printf("New socket buffer size: %d\n", check);

    return sockfd;
}

int epoll_setup(struct epoll_event* setup)
{
    int rc, epollfd = epoll_create1(0);
    if (epollfd == -1)
    {
        perror("epoll_create");
        return -1;
    }
    setup->events = EPOLLIN | EPOLLHUP;
    return epollfd;
}

int epoll_add_fd(int epollfd, int fd, struct epoll_event* setup)
{
    setup->data.fd = fd;
    setup->events = EPOLLIN | EPOLLHUP;
    int rc = epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, setup);
    if (rc == -1)
    {
        perror("error_ctl");
        return -1;
    }
    return 0;
}