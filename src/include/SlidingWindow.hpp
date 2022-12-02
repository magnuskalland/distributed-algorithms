#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/timerfd.h>

#include <sys/types.h>
#include <stddef.h>

#include <cmath>

#include "CircularBuffer.hpp"
#include "Timer.hpp"
#include "packets.hpp"
#include "network_utils.hpp"

class SlidingWindow
{
public:
    inline SlidingWindow(int socket, uint64_t id, uint64_t destinationId, in_addr_t destinationIp,
        uint16_t destinationPort, uint32_t windowSize,
        uint32_t messagesPerPacket, uint32_t sequenceNumberOfLastPacket)
    {
        this->socket = socket;
        this->id = id;
        this->destinationId = destinationId;
        this->windowSize = windowSize;
        this->messagesPerPacket = messagesPerPacket;
        this->sequenceNumberOfLastPacket = sequenceNumberOfLastPacket;

        setuphost(destinationAddress, destinationIp, destinationPort);
        bzero(&messageSequencePlaceholder, sizeof(struct MessageSequence));

        pthread_mutex_init(&mutex, NULL);

        messageSequencePlaceholder.sender = id;
        messageSequencePlaceholder.numberOfPackets = messagesPerPacket;
    }

    virtual inline ~SlidingWindow() {
        // printStatistics();
        pthread_mutex_destroy(&mutex);
    }

    inline uint64_t getId()
    {
        return id;
    }

    pthread_mutex_t* getLock()
    {
        return &mutex;
    }

    inline void lock()
    {
        pthread_mutex_lock(&mutex);
    }

    inline void unlock()
    {
        pthread_mutex_unlock(&mutex);
    }

    inline uint32_t getWindowSize() const
    {
        return windowSize;
    }

    inline uint64_t getDestinationId()
    {
        return destinationId;
    }

protected:
    virtual inline uint32_t shiftWindow() = 0;
    virtual uint32_t windowStart() = 0;
    virtual inline uint32_t windowEnd() = 0;
    virtual inline bool duplicate(uint32_t sequenceNumber) = 0;
    virtual inline void printStatistics() = 0;

    inline ssize_t sendSequence(struct MessageSequence* sequence)
    {
        char* serialiedSequence;
        ssize_t size, wc;
        size = serialize_sequence(&serialiedSequence, sequence);
        // if (payload(sequence->type))
        // {
        //     if (getDestinationId() == id)
        //         printf("PAYLOAD %d to self\n", sequence->sequenceNumber);
        //     else
        //         printf("PAYLOAD %d to %ld\n", sequence->sequenceNumber, destinationId);
        // }

        // else if (ack(sequence->type))
        // {
        //     if (getDestinationId() == id)
        //         printf("ACK %d to self\n", sequence->sequenceNumber);
        //     else
        //         printf("ACK %d to %ld\n", sequence->sequenceNumber, destinationId);
        // }

        // else if (forward(sequence->type))
        // {
        //     if (getDestinationId() == id)
        //         printf("FORWARD (%ld, %d) to self\n",
        //             sequence->sender, sequence->sequenceNumber);
        //     else
        //         printf("FORWARD (%ld, %d) to %ld\n",
        //             sequence->sender, sequence->sequenceNumber, destinationId);
        // }

        // else if (delivery(sequence->type))
        // {
        //     if (getDestinationId() == id)
        //         printf("DELIVERY (%ld, %d) to self\n",
        //             sequence->sender, sequence->sequenceNumber);
        //     else
        //         printf("DELIVERY (%ld, %d) to %ld\n",
        //             sequence->sender, sequence->sequenceNumber, destinationId);
        // }

        wc = sendto(socket,
            serialiedSequence, size, 0,
            reinterpret_cast<sockaddr*>(&destinationAddress), sizeof(destinationAddress));
        if (wc == -1)
        {
            fprintf(stderr, "sendto(%d, %d, %ld, ...)\n", socket, sequence->sequenceNumber, size);
            perror("sendto");
            traceerror();
            return -1;
        }

        free(serialiedSequence);
        return wc;
    }

    inline bool sequenceLeftOfWindow(const uint32_t sequenceNumber)
    {
        return sequenceNumber < windowStart();
    }



    uint64_t id, destinationId;
    int socket;

    sockaddr_in destinationAddress;
    uint32_t windowSize;
    uint32_t messagesPerPacket;
    uint32_t sequenceNumberOfLastPacket;

    pthread_mutex_t mutex;

    struct MessageSequence messageSequencePlaceholder;
    char buffer[PACKED_MESSAGE_SEQUENCE_SIZE] = { 0 };

    bool t = true;
    bool f = false;

    struct Statistics
    {
        uint32_t correctTransmits = 0;
        uint32_t duplicatePackets = 0;
        uint32_t timeouts = 0;
    } statistics;
};