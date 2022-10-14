#pragma once

#include <stdint.h>

#define MESSAGES_PER_PACKET     8
#define WINDOW_SIZE             32
#define TIMEOUT_SEC             0
#define TIMEOUT_NANO            50000000

struct PerformanceConfig
{
    uint32_t messagesPerPacket;
    uint32_t windowSize;
    uint32_t timeoutSec;
    uint32_t timeoutNano;
};