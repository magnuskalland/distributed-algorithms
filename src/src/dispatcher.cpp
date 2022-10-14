#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <signal.h>

#include <vector>

#include "PacketQueue.hpp"
#include "Logger.hpp"

#include "dispatcher.hpp"
#include "packets.hpp"
#include "macros.hpp"
#include "parser.hpp"
#include "poll.hpp"

int dispatch(const Parser::Host host, std::vector<PacketQueue<char*>*>& qs)
{
    int sockfd, epollfd;
    ssize_t rc, wc;
    uint64_t sender;

    std::vector<PacketQueue<char*>*> queues = qs;
    char* buf;

    socklen_t addrlen = sizeof(sockaddr);
    struct sockaddr_in server;
    struct sockaddr src_addr;
    struct epoll_event event, setup;

    std::cout << "Receiving on " << host.ipReadable() << ":" << host.portReadable() << "\n";

    sockfd = SOCKET();
    if (sockfd == -1)
    {
        traceerror();
        return EXIT_FAILURE;
    }

    setuphost(server, host.ip, host.port);

    rc = bind(sockfd, reinterpret_cast<sockaddr*>(&server), sizeof(server));
    if (rc == -1)
    {
        traceerror();
        return EXIT_FAILURE;
    }

    epollfd = epoll_create1(0);
    if (epollfd == -1)
    {
        traceerror();
        close(sockfd);
        return EXIT_FAILURE;
    }

    rc = epoll_add_fd(epollfd, sockfd, &setup);
    if (rc == -1)
    {
        traceerror();
        close(sockfd);
        close(epollfd);
        return EXIT_FAILURE;
    }

    while (true)
    {
        buf = static_cast<char*>(malloc(PACKED_MESSAGE_SEQUENCE_SIZE + addrlen));
        if (!buf)
        {
            perror("malloc");
            close(sockfd);
            close(epollfd);
            return EXIT_FAILURE;
        }

        rc = epoll_wait(epollfd, &event, 1, -1);
        if (rc == -1)
        {
            perror("epoll_wait");
            traceerror();
            close(sockfd);
            close(epollfd);
            return EXIT_FAILURE;
        }

        rc = recvfrom(sockfd, buf, PACKED_MESSAGE_SEQUENCE_SIZE, 0, &src_addr, &addrlen);
        if (rc <= 0)
        {
            perror("recvfrom");
            traceerror();
            return EXIT_FAILURE;
        }

        memcpy(&buf[PACKED_MESSAGE_SEQUENCE_SIZE], &src_addr, addrlen);
        memcpy(&sender, &buf[offsetof(struct MessageSequence, sender)], sizeof(uint64_t));
        queues[sender - 1]->push_msg(buf);
    }

    return EXIT_SUCCESS;
}