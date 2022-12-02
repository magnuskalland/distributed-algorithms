#pragma once

#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <stdio.h>
#include <unistd.h>

static uint32_t open_files = 0;

inline int get_socket()
{
    return socket(AF_INET, SOCK_DGRAM, 0);
}

inline int get_socket(uint64_t buffer_size)
{
    int sockfd, err;
    uint size_of_socket_buf_size = sizeof(buffer_size);

    uint64_t check;

    sockfd = get_socket();
    if (sockfd == -1)
    {
        perror("socket");
        return -1;
    }

    // getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<void*>(&check), &size_of_socket_buf_size);
    // printf("Original socket buffer size: %ld\n", check);

    err = setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF,
        reinterpret_cast<void*>(&buffer_size), size_of_socket_buf_size);
    if (err == -1)
    {
        perror("setsockopt");
        close(sockfd);
        return -1;
    }
    // getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<void*>(&check), &size_of_socket_buf_size);
    // printf("New socket buffer size: %ld\n", check);

    return sockfd;
}

inline int epoll_setup(struct epoll_event* setup)
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

inline int epoll_add_fd(int epollfd, int fd, struct epoll_event* setup)
{
    setup->data.fd = fd;
    setup->events = EPOLLIN;
    int rc = epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, setup);
    if (rc == -1)
    {
        perror("epoll_ctl");
        return -1;
    }
    return 0;
}

inline int get_next_packet_size(int sockfd)
{
    int size;
    ioctl(sockfd, FIONREAD, &size);
    return size;
}

inline int get_socket_size(int sockfd)
{
    uint32_t size;
    uint size_size = sizeof(size);
    getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<void*>(&size), &size_size);
    return size;
}