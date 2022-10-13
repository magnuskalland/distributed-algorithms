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
        uint32_t lastPacketSequenceNumber)
    {
        this->socket = socket;
        this->windowSize = windowSize;
        this->lastPacketSequenceNumber = lastPacketSequenceNumber;

        packetPlaceholder.sender = sourceId;
        packetPlaceholder.payload.data = dataPlaceholder;

        outOfOrderMessageBuffer = new CircularBuffer<struct PerfectLinksPacket*>(windowSize);
        deliverableMessages = new CircularBuffer<struct PerfectLinksPacket*>(windowSize);
    }
    ~SlidingWindow()
    {
        outOfOrderMessageBuffer->~CircularBuffer();
        deliverableMessages->~CircularBuffer();
    }

    uint32_t shiftWindow()
    {
        uint32_t shift;
        shift = outOfOrderMessageBuffer->getShiftCounter();
        if (shift == 0)
        {
            return 0;
        }

        for (uint32_t i = windowStart(); i < windowStart() + shift; i++)
        {
            deliverableMessages->insert(outOfOrderMessageBuffer->remove(i), i);
        }
        outOfOrderMessageBuffer->shift(shift);
        return shift;
    }

    inline bool notFinished()
    {
        return statistics.correctTransmits < lastPacketSequenceNumber;
    }

    inline uint32_t getWindowSize()
    {
        return windowSize;
    }

    inline uint32_t windowStart()
    {
        return outOfOrderMessageBuffer->getStart();
    }

    inline uint32_t windowEnd()
    {
        return outOfOrderMessageBuffer->getEnd();
    }

    inline bool duplicate(struct PerfectLinksPacket* packet)
    {
        return outOfOrderMessageBuffer->get(packet->seqnr)
            || packetLeftOfWindow(packet);
    }
    inline struct PerfectLinksPacket* getMessageToDeliver()
    {
        return deliverableMessages->pop();
    }
    CircularBuffer<struct PerfectLinksPacket*>* deliverableMessages;

protected:
    inline ssize_t sendPacket(struct PerfectLinksPacket* packet, size_t size)
    {
        ssize_t wc = sendto(socket, reinterpret_cast<char*>(packet),
            size, 0, reinterpret_cast<sockaddr*>(&destinationAddress),
            sizeof(destinationAddress));
        if (wc == -1)
        {
            perror("sendto");
        }
        return wc;
    }

    inline ssize_t receivePacket(struct PerfectLinksPacket* packet)
    {
        ssize_t rc = recv(socket, buffer, ACK_PACKET_SIZE, 0);
        if (rc == -1)
        {
            perror("recv");
            return -1;
        }
        deserialize_packet(packet, buffer); // heap allocates packet->payload.data
        return rc;
    }

    inline bool packetLeftOfWindow(struct PerfectLinksPacket* packet)
    {
        return packet->seqnr < windowStart();
    }

    int socket;
    sockaddr_in destinationAddress;

    uint32_t windowSize;
    uint32_t lastPacketSequenceNumber;

    struct PerfectLinksPacket packetPlaceholder;
    char dataPlaceholder[MAX_PAYLOAD_SIZE] = { 0 };
    char buffer[MESSAGE_PACKET_SIZE] = { 0 };

    CircularBuffer<struct PerfectLinksPacket*>* outOfOrderMessageBuffer;


    struct Statistics
    {
        uint32_t correctTransmits = 0;
        uint32_t duplicatePackets = 0;
        uint32_t timeouts = 0;
    } statistics;
};