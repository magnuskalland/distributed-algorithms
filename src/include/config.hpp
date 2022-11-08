#pragma once

#include <stdint.h>
#include <stdio.h>

#define MESSAGES_PER_PACKET     8
#define WINDOW_SIZE             1000
#define TIMEOUT_SEC             0
#define TIMEOUT_NANO            50000000

class PerformanceConfig
{
public:
    PerformanceConfig(uint32_t numberOfMessagesToBeSent, uint32_t unitSize)
    {
        messagesPerPacket = MESSAGES_PER_PACKET;
        windowSize = WINDOW_SIZE * MESSAGES_PER_PACKET > numberOfMessagesToBeSent ?
            numberOfMessagesToBeSent / MESSAGES_PER_PACKET + 1 : WINDOW_SIZE;
        timeoutSec = TIMEOUT_SEC;
        timeoutNano = TIMEOUT_NANO;
        socketBufferSize = windowSize * unitSize + 1000;
    }

    void print()
    {
        printf("%-22s: %d\n%-22s: %d\n%-22s: %d bytes\n%-22s: %.2f ms\nYou can change configuration settings with macros in \n'%s'\n\n",
            "Messages per sequence", messagesPerPacket,
            "Sequences per window", windowSize,
            "Socket buffer size", socketBufferSize,
            "Timeout", static_cast<float>(timeoutNano) / 1000000,
            __FILE__);
    }

    inline uint32_t getMessagesPerPacket()
    {
        return messagesPerPacket;
    }
    inline uint32_t getWindowSize()
    {
        return windowSize;
    }
    inline uint32_t getSocketBufferSize()
    {
        return socketBufferSize;
    }
    inline uint32_t getTimeoutSec()
    {
        return timeoutSec;
    }
    inline uint32_t getTimeoutNano()
    {
        return timeoutNano;
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
    uint32_t socketBufferSize;

    /*
     * Timeout in seconds before packet retransmission is triggered.
    */
    uint32_t timeoutSec;

    /*
     * Timeout in nanoseconds before packet retransmission is triggered.
     * Is added to the number of seconds.
    */
    uint32_t timeoutNano;
};