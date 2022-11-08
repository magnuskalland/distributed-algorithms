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

#include "threads.hpp"
#include "packets.hpp"
#include "macros.hpp"
#include "parser.hpp"
#include "network_utils.hpp"

int sockfd, epollfd;
char* buf;
int tid;
struct DispatcherArgs* args;

static void clean(int)
{
    close(sockfd);
    close(epollfd);

    pthread_exit(NULL);
}

void* dispatch(void* ptr)
{
    assign_handler(clean);
    args = reinterpret_cast<struct DispatcherArgs*>(ptr);

    ssize_t rc, wc;
    uint64_t sender;

    std::vector<PacketQueue<char*>*> queues = args->queues;

    socklen_t addrlen = sizeof(sockaddr);
    struct sockaddr_in server;
    struct sockaddr src_addr;
    struct epoll_event events, setup;

    bzero(&events, sizeof(epoll_event));
    bzero(&setup, sizeof(epoll_event));

    std::cout << "Receiving on " << args->host.ipReadable() << ":" << args->host.portReadable() << "\n";

    sockfd = get_socket(args->config->getSocketBufferSize());
    if (sockfd == -1)
    {
        traceerror();
        pthread_exit(NULL);
    }

    setuphost(server, args->host.ip, args->host.port);

    rc = bind(sockfd, reinterpret_cast<sockaddr*>(&server), sizeof(server));
    if (rc == -1)
    {
        perror("bind");
        traceerror();
        pthread_exit(NULL);
    }

    /* prepare epoll */
    epollfd = epoll_setup(&setup);
    if (epollfd == -1)
    {
        traceerror();
        pthread_exit(NULL);
    }

    rc = epoll_add_fd(epollfd, sockfd, &setup);
    if (rc == -1)
    {
        traceerror();
        close(sockfd);
        pthread_exit(NULL);
    }

    while (true)
    {
        buf = static_cast<char*>(malloc(PACKED_MESSAGE_SEQUENCE_SIZE + addrlen));
        if (!buf)
        {
            perror("malloc");
            close(sockfd);
            close(epollfd);
            pthread_exit(NULL);
        }

        rc = epoll_wait(epollfd, &events, 1, -1);
        if (rc == -1)
        {
            free(buf);
            if (errno == EINTR)
            {
                continue;
            }
            perror("epoll_wait");
            close(sockfd);
            close(epollfd);
            pthread_exit(NULL);
        }

        rc = recvfrom(sockfd, buf, PACKED_MESSAGE_SEQUENCE_SIZE, 0, &src_addr, &addrlen);
        if (rc <= 0)
        {
            perror("recvfrom");
            free(buf);
            close(sockfd);
            close(epollfd);
            pthread_exit(NULL);
        }

        memcpy(&buf[PACKED_MESSAGE_SEQUENCE_SIZE], &src_addr, addrlen);
        memcpy(&sender, &buf[offsetof(struct MessageSequence, sender)], sizeof(uint64_t));
        queues[sender - 1]->pushMessage(buf);
    }
    clean(0);
    pthread_exit(NULL);
}