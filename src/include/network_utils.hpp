#pragma once

#include <sys/epoll.h>

int get_socket(uint32_t buffer_size);
int epoll_setup(struct epoll_event* setup);
int epoll_add_fd(int epollfd, int fd, struct epoll_event* setup);