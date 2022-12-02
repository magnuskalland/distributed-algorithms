#pragma once

#include <stdint.h>
#include <stdio.h>

#define MESSAGES_PER_PACKET     1
#define WINDOW_SIZE             3
#define TIMEOUT_SEC             3
#define TIMEOUT_NANO            50000000
#define SELF_DELIVERY           true

class PerformanceConfig
{
public:
    PerformanceConfig(uint32_t n_messages, uint32_t unitSize, uint64_t network_size)
    {
        messagesPerPacket = MESSAGES_PER_PACKET;
        windowSize = WINDOW_SIZE * MESSAGES_PER_PACKET > n_messages ?
            MESSAGES_PER_PACKET >= n_messages ?
            1 :
            n_messages / MESSAGES_PER_PACKET + 1
            : WINDOW_SIZE;
        timeoutSec = TIMEOUT_SEC;
        timeoutNano = TIMEOUT_NANO;
        socketBufferSize = windowSize * unitSize * (network_size * network_size * 10);
        selfDelivery = SELF_DELIVERY;
    }

    void print()
    {
        printf("%-30s: %d\n%-30s: %d\n%-30s: min %ld bytes\n%-30s: %.2fms\n%-30s: %s\nEdit configuration settings in '%s'\n\n",
            "Messages per sequence", messagesPerPacket,
            "Sequences per window", windowSize,
            "Socket buffer size", socketBufferSize,
            "Timeout", static_cast<float>(timeoutNano) / 1000000,
            "Self delivery", selfDelivery ? "True" : "False",
            __FILE__);
    }

    uint32_t getMessagesPerPacket()
    {
        return messagesPerPacket;
    }
    uint32_t getWindowSize()
    {
        return windowSize;
    }
    uint64_t getSocketBufferSize()
    {
        return socketBufferSize;
    }
    uint32_t getTimeoutSec()
    {
        return timeoutSec;
    }
    uint32_t getTimeoutNano()
    {
        return timeoutNano;
    }
    bool getSelfDelivery()
    {
        return selfDelivery;
    }

private:

    /**
     * How many messages we can fit into a single sequence/packet
    */
    uint32_t messagesPerPacket;

    /*
     * Size of window in terms of number of messages.
     * Is set to min(WINDOW_SIZE, the number of messages to be sent).
    */
    uint32_t windowSize;

    /**
     * The socket buffer size we manually set the sockets with by using setsockopt.
     * Will be a product of packet size in bytes and windowSize.
    */
    uint64_t socketBufferSize;

    /*
     * Timeout in seconds before packet retransmission is triggered.
    */
    uint32_t timeoutSec;

    /*
     * Timeout in nanoseconds before packet retransmission is triggered.
     * Is added to the number of seconds.
    */
    uint32_t timeoutNano;

    /*
     * Determines if host A also delivers to host A.
    */
    bool selfDelivery;
};