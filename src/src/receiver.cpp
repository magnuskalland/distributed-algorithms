#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stddef.h>
#include <signal.h>

#include <string>

#include "receiver.hpp"
#include "macros.hpp"

#include "Logger.hpp"
#include "PacketQueue.hpp"
#include "ReceiverWindow.hpp"

int receive(Logger& logger, struct PerformanceConfig& config,
    PacketQueue<char*>*& queue, uint64_t id, uint32_t numberOfMessagesToBeSent)
{
    int sockfd;
    ssize_t wc;
    struct MessageSequence* sequence;

    char log_entry[LOG_MSG_SIZE];

    ReceiverWindow* window;
    uint32_t shift;

    char* message;

    sockfd = SOCKET();
    if (sockfd == -1)
    {
        traceerror();
        return EXIT_FAILURE;
    }

    window = new ReceiverWindow(sockfd, id, config.windowSize,
        config.messagesPerPacket, numberOfMessagesToBeSent);

    /* loop forever because the last ack may get lost */
    while (true)
    {
        message = queue->get_msg(); // blocking call, returns a heap allocated memory area

        /* send ack */
        wc = window->sendAcknowledgement(message);
        if (wc == -1)
        {
            free(message);
            traceerror();
            return EXIT_FAILURE;
        }

        /* deliver messages */
        while ((sequence = window->getMessagesToDeliver()))
        {
            for (uint32_t i = 0; i < sequence->numberOfPackets; i++)
            {
                sprintf(log_entry, RECEIVER_FORMAT,
                    sequence->sender, sequence->payload[i].sequenceNumber);
                logger.log(log_entry);
            }
            free_sequence(sequence);
        }
        free(message);
    }
    close(sockfd);
    return 0;
}