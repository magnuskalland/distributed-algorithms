#pragma once

#include "parser.hpp"
#include "Logger.hpp"
#include "packets.hpp"

int send_to_host(Logger& logger, Parser::PerformanceConfig& config,
    uint64_t src, Parser::Host dest, uint32_t n_messages);
