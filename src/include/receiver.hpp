#pragma once

#include "PacketQueue.hpp"
#include "Logger.hpp"
#include "parser.hpp"

int receive(Logger& logger, uint32_t windowSize, PacketQueue<char*>*& queue,
    uint64_t id, uint32_t n_messages);