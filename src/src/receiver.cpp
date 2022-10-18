#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stddef.h>
#include <signal.h>

#include <thread>

#include "threads.hpp"
#include "macros.hpp"

#include "Logger.hpp"
#include "PacketQueue.hpp"
#include "ReceiverWindow.hpp"

static void clean(int)
{
    pthread_exit(0);
}

void* receive(void* ptr)
{
    ssize_t wc;
    struct MessageSequence* sequence;
    char log_entry[LOG_MSG_SIZE];
    char* message;

    assign_handler(clean);
    struct ReceiverArgs* args = reinterpret_cast<struct ReceiverArgs*>(ptr);

    *(args->sockfd) = SOCKET();
    if (*args->sockfd == -1)
    {
        perror("socket");
        traceerror();
        pthread_exit(0);
    }

    ReceiverWindow window = ReceiverWindow(*args->sockfd, args->id, args->config.windowSize,
        args->config.messagesPerPacket, args->numberOfMessagesToBeSent);

    /* loop forever because the last ack may get lost */
    while (true)
    {
        message = args->queue->popMessage(); // blocking call, returns a heap allocated memory area

        /* send ack */
        wc = window.sendAcknowledgement(message);
        if (wc == -1)
        {
            free(message);
            traceerror();
            pthread_exit(0);
        }

        /* deliver messages */
        while ((sequence = window.getMessagesToDeliver()))
        {
            for (uint32_t i = 0; i < sequence->numberOfPackets; i++)
            {
                sprintf(log_entry, RECEIVER_FORMAT,
                    sequence->sender, sequence->payload[i].sequenceNumber);
                args->logger.log(log_entry);
            }
            freesequencedata(sequence);
            free(sequence);
        }
        free(message);
    }
    pthread_exit(0);
}