#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>

#include <unistd.h>
#include <stdio.h>

#include <string>

#include "sender.hpp"
#include "parser.hpp"
#include "macros.hpp"
#include "poll.hpp"
#include "config.hpp"

#include "Logger.hpp"
#include "Timer.hpp"
#include "SenderWindow.hpp"

int send_to_host(Logger& logger, PerformanceConfig& config,
    uint64_t src, Parser::Host dest, uint32_t numberOfMessagesToBeSent)
{
    uint32_t timed_out_seq_nr;
    ssize_t wc, rc;
    int sockfd, epollfd;
    struct epoll_event events, setup;
    char log_entry[LOG_MSG_SIZE];

    SenderWindow* window;
    Timer* timer;
    struct MessageSequence* sequence;

    timer = new Timer(config.windowSize,
        config.timeoutSec, config.timeoutNano);

    sockfd = SOCKET();
    if (sockfd == -1)
    {
        perror("socket");
        traceerror();
        return EXIT_FAILURE;
    }

    std::cout << "Setting up destination " << dest.ipReadable() << ":" << dest.portReadable() << "\n";
    window = new SenderWindow(sockfd, src, dest.ip, dest.port,
        config.windowSize, config.messagesPerPacket, numberOfMessagesToBeSent);

    /* prepare epoll */
    epollfd = epoll_setup(&setup);
    if (epollfd == -1)
    {
        traceerror();
        return EXIT_FAILURE;
    }

    /* add fds to epoll */
    epoll_add_fd(epollfd, sockfd, &setup);
    for (uint32_t i = 0; i < config.windowSize; i++)
    {
        epoll_add_fd(epollfd, timer->getTimerFd(i + 1), &setup);
    }

    /* send the whole window to initiate transmit */
    wc = window->initiateTransmission(timer);
    if (wc == -1)
    {
        return EXIT_FAILURE;
    }

    // while (window->notFinished())
    while (true)
    {

        /* blocking */
        rc = epoll_wait(epollfd, &events, 1, -1);
        if (rc == -1)
        {
            traceerror();
            return EXIT_FAILURE;
        }

        /* ack received */
        if (events.data.fd == sockfd)
        {
            rc = window->receiveAcknowledgementSequence(timer);
            if (rc == -1)
            {
                traceerror();
                return EXIT_FAILURE;
            }

            while ((sequence = window->getMessagesToDeliver()))
            {
                for (uint32_t i = 0; i < sequence->numberOfPackets; i++)
                {
                    sprintf(log_entry, SENDER_FORMAT,
                        sequence->payload[i].sequenceNumber);
                    logger.log(log_entry);
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
            return EXIT_FAILURE;
        }
        wc = timer->armTimer(timed_out_seq_nr, ARM);
        if (wc == -1)
        {
            traceerror();
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

