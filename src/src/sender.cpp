#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>

#include <unistd.h>
#include <stdio.h>

#include "threads.hpp"
#include "parser.hpp"
#include "macros.hpp"
#include "poll.hpp"
#include "config.hpp"

#include "Logger.hpp"
#include "Timer.hpp"
#include "SenderWindow.hpp"

static void clean(int)
{

    // free(buf);
    // close(sockfd);
    // close(epollfd);

    printf("Sender exiting\n");
    pthread_exit(0);
}

void* send(void* ptr)
{
    uint32_t timed_out_seq_nr;
    ssize_t wc, rc;
    int sockfd, epollfd;
    struct epoll_event events, setup;
    char log_entry[LOG_MSG_SIZE];

    SenderWindow* window;
    Timer* timer;
    struct MessageSequence* sequence;

    assign_handler(clean);
    struct SenderArgs* args = reinterpret_cast<struct SenderArgs*>(ptr);

    timer = new Timer(args->config.windowSize,
        args->config.timeoutSec, args->config.timeoutNano);

    sockfd = SOCKET();
    if (sockfd == -1)
    {
        perror("socket");
        traceerror();
        pthread_exit(0);
    }

    std::cout << "Setting up destination " << args->dest.ipReadable() << ":" << args->dest.portReadable() << "\n";
    window = new SenderWindow(sockfd, args->src, args->dest.ip, args->dest.port,
        args->config.windowSize, args->config.messagesPerPacket, args->numberOfMessagesToBeSent);

    /* prepare epoll */
    epollfd = epoll_setup(&setup);
    if (epollfd == -1)
    {
        traceerror();
        pthread_exit(0);
    }

    /* add fds to epoll */
    epoll_add_fd(epollfd, sockfd, &setup);
    for (uint32_t i = 0; i < args->config.windowSize; i++)
    {
        epoll_add_fd(epollfd, timer->getTimerFd(i + 1), &setup);
    }

    /* send the whole window to initiate transmit */
    wc = window->initiateTransmission(timer);
    if (wc == -1)
    {
        pthread_exit(0);
    }

    // while (window->notFinished())
    while (true)
    {
        /* blocking */
        rc = epoll_wait(epollfd, &events, 1, -1);
        if (rc == -1)
        {
            traceerror();
            pthread_exit(0);
        }

        /* ack received */
        if (events.data.fd == sockfd)
        {
            rc = window->receiveAcknowledgementSequence(timer);
            if (rc == -1)
            {
                traceerror();
                pthread_exit(0);
            }

            while ((sequence = window->getMessagesToDeliver()))
            {
                for (uint32_t i = 0; i < sequence->numberOfPackets; i++)
                {
                    sprintf(log_entry, SENDER_FORMAT,
                        sequence->payload[i].sequenceNumber);
                    args->logger.log(log_entry);
                }
                free_sequence(sequence);
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
            pthread_exit(0);
        }
        wc = timer->armTimer(timed_out_seq_nr, ARM);
        if (wc == -1)
        {
            traceerror();
            pthread_exit(0);
        }
    }
    pthread_exit(0);
}

