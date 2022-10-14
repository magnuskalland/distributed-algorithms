#pragma once

#include "PacketQueue.hpp"
#include "Logger.hpp"
#include "parser.hpp"

int receive(Logger& logger, struct PerformanceConfig& config,
    PacketQueue<char*>*& queue, uint64_t id, uint32_t numberOfMessagesToBeSent);