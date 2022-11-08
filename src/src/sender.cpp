#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>

#include <unistd.h>
#include <stdio.h>

#include "threads.hpp"
#include "parser.hpp"
#include "macros.hpp"
#include "network_utils.hpp"
#include "config.hpp"

#include "Logger.hpp"
#include "Timer.hpp"
#include "SenderWindow.hpp"

static int sockfd, epollfd;
Timer* timer;
SenderWindow* window;

static void clean(int)
{
    close(sockfd);
    close(epollfd);

    delete(timer);
    delete(window);

    pthread_exit(NULL);
}

void* send(void* ptr)
{
    uint32_t timed_out_seq_nr;
    ssize_t wc, rc;
    struct epoll_event events, setup;
    char log_entry[LOG_MSG_SIZE];

    struct MessageSequence* sequence;

    bzero(&events, sizeof(struct epoll_event));
    bzero(&setup, sizeof(struct epoll_event));

    assign_handler(clean);
    struct SenderArgs* args = reinterpret_cast<struct SenderArgs*>(ptr);

    timer = new Timer(args->config->getWindowSize(), args->config->getTimeoutSec(), args->config->getTimeoutNano());

    sockfd = get_socket(args->config->getSocketBufferSize());
    if (sockfd == -1)
    {
        traceerror();
        pthread_exit(NULL);
    }

    std::cout << "Setting up destination " << args->dest.ipReadable() << ":" << args->dest.portReadable() << "\n";

    window = new SenderWindow(sockfd, args->src, args->dest.ip, args->dest.port,
        args->config->getWindowSize(), args->config->getMessagesPerPacket(), args->numberOfMessagesToBeSent);

    /* prepare epoll */
    epollfd = epoll_setup(&setup);
    if (epollfd == -1)
    {
        traceerror();
        pthread_exit(NULL);
    }

    /* add fds to epoll */
    epoll_add_fd(epollfd, sockfd, &setup);
    for (uint32_t i = 0; i < args->config->getWindowSize(); i++)
    {
        epoll_add_fd(epollfd, timer->getTimerFd(i + 1), &setup);
    }

    /* send the whole window to initiate transmit */
    wc = window->initiateTransmission(timer);
    if (wc == -1)
    {
        pthread_exit(NULL);
    }

    while (true)
    {
        /* blocking */
        rc = epoll_wait(epollfd, &events, 1, -1);
        if (rc == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("epoll_wait");
            pthread_exit(NULL);
        }

        /* ack received */
        if (events.data.fd == sockfd)
        {
            rc = window->receiveAcknowledgementSequence(timer);
            if (rc == -1)
            {
                traceerror();
                pthread_exit(NULL);
            }

            while ((sequence = window->getMessagesToDeliver()))
            {
                for (uint32_t i = 0; i < sequence->numberOfPackets; i++)
                {
                    sprintf(log_entry, SENDER_FORMAT,
                        sequence->payload[i].sequenceNumber);
                    args->logger.log(log_entry);
                }
                freesequencedata(sequence);
                free(sequence);
            }

            continue;
        }

        /* timeout */
        timed_out_seq_nr = timer->matchTimer(events.data.fd);
        if (timed_out_seq_nr == 0)
        {
            continue;
        }

        window->incrementTimeouts();
        wc = window->sendPayloadSequence(timed_out_seq_nr);
        if (wc == -1)
        {
            traceerror();
            pthread_exit(NULL);
        }
        wc = timer->armTimer(timed_out_seq_nr, ARM);
        if (wc == -1)
        {
            traceerror();
            pthread_exit(NULL);
        }
    }
    pthread_exit(NULL);
}

