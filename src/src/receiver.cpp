#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stddef.h>
#include <signal.h>

#include <string>

#include "parser.hpp"
#include "receiver.hpp"
#include "macros.hpp"

#include "Logger.hpp"
#include "PacketQueue.hpp"
#include "ReceiverWindow.hpp"

int receive(Logger& logger, uint32_t windowSize, PacketQueue<char*>*& queue, uint64_t id, uint32_t n_messages)
{
    int sockfd;
    ssize_t wc;
    struct PerfectLinksPacket* packet;

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

    window = new ReceiverWindow(sockfd, id, windowSize, n_messages);

    /* loop forever because the last ack may get lost */
    while (true)
    {
        message = queue->get_msg(); // blocking call, return a heap allocated memory area
        /* send ack */
        wc = window->sendAcknowledgement(message);
        if (wc == -1)
        {
            free(message);
            traceerror();
            return EXIT_FAILURE;
        }

        /* deliver messages */
        while ((packet = window->getMessageToDeliver()))
        {
            sprintf(log_entry, "d %ld %d\n", packet->sender, packet->seqnr);
            logger.log(log_entry);
            free_packet(packet);
        }
        free(message);
    }
    close(sockfd);
    return 0;
}