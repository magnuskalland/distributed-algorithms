#include <sys/epoll.h>
#include "poll.hpp"

int epoll_setup(struct epoll_event* setup)
{
    int rc, epollfd = epoll_create1(0);
    if (epollfd == -1) return -1;
    setup->events = EPOLLIN | EPOLLHUP;
    return epollfd;
}

int epoll_add_fd(int epollfd, int fd, struct epoll_event* setup)
{
    setup->data.fd = fd;
    setup->events = EPOLLIN | EPOLLHUP;
    int rc = epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, setup);
    if (rc == -1) return -1;
    return 0;
}