#pragma once

#include "parser.hpp"
#include "Logger.hpp"
#include "config.hpp"

int send_to_host(Logger& logger, PerformanceConfig& config,
    uint64_t src, Parser::Host dest, uint32_t numberOfMessagesToBeSent);
