#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/timerfd.h>

#include <sys/types.h>
#include <stddef.h>

#include "CircularBuffer.hpp"
#include "Timer.hpp"
#include "packets.hpp"

class SlidingWindow
{
public:
    SlidingWindow(int socket, uint64_t sourceId, uint32_t windowSize,
        uint32_t messagesPerPacket, uint32_t sequenceNumberOfLastPacket)
    {
        this->socket = socket;
        this->windowSize = windowSize;
        this->messagesPerPacket = messagesPerPacket;
        this->sequenceNumberOfLastPacket = sequenceNumberOfLastPacket;

        messageSequencePlaceholder.sender = sourceId;
        messageSequencePlaceholder.numberOfPackets = messagesPerPacket;

        messageSequencesOutOfOrder = new CircularBuffer<struct MessageSequence*>(windowSize);
        deliverableMessages = new CircularBuffer<struct MessageSequence*>(windowSize);
    }
    ~SlidingWindow()
    {
        messageSequencesOutOfOrder->~CircularBuffer();
        deliverableMessages->~CircularBuffer();
    }

    uint32_t shiftWindow()
    {
        uint32_t shift;
        shift = messageSequencesOutOfOrder->getShiftCounter();
        if (shift == 0)
        {
            return 0;
        }

        for (uint32_t i = windowStart(); i < windowStart() + shift; i++)
        {
            deliverableMessages->insert(messageSequencesOutOfOrder->remove(i), i);
        }
        messageSequencesOutOfOrder->shift(shift);
        return shift;
    }

    inline bool notFinished()
    {
        return statistics.correctTransmits < sequenceNumberOfLastPacket;
    }

    inline uint32_t getWindowSize()
    {
        return windowSize;
    }

    inline uint32_t windowStart()
    {
        return messageSequencesOutOfOrder->getStart();
    }

    inline uint32_t windowEnd()
    {
        return messageSequencesOutOfOrder->getEnd();
    }

    inline bool duplicate(struct MessageSequence* sequence)
    {
        return messageSequencesOutOfOrder->get(sequence->sequenceNumber)
            || sequenceLeftOfWindow(sequence);
    }
    inline struct MessageSequence* getMessagesToDeliver()
    {
        return deliverableMessages->pop();
    }

protected:
    inline ssize_t sendSequence(struct MessageSequence* sequence)
    {
        char* serialiedSequence;
        ssize_t size;

        size = serialize_sequence(&serialiedSequence, sequence);
        ssize_t wc = sendto(socket,
            serialiedSequence, size,
            0,
            reinterpret_cast<sockaddr*>(&destinationAddress), sizeof(destinationAddress));
        if (wc == -1)
        {
            perror("sendto");
        }
        free(serialiedSequence);
        return wc;
    }

    inline bool sequenceLeftOfWindow(struct MessageSequence* sequence)
    {
        return sequence->sequenceNumber < windowStart();
    }

    int socket;
    sockaddr_in destinationAddress;

    uint32_t windowSize;
    uint32_t messagesPerPacket;
    uint32_t sequenceNumberOfLastPacket;

    struct MessageSequence messageSequencePlaceholder;
    char buffer[PACKED_MESSAGE_SEQUENCE_SIZE];

    CircularBuffer<struct MessageSequence*>* messageSequencesOutOfOrder;
    CircularBuffer<struct MessageSequence*>* deliverableMessages;

    struct Statistics
    {
        uint32_t correctTransmits = 0;
        uint32_t duplicatePackets = 0;
        uint32_t timeouts = 0;
    } statistics;
};