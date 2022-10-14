#pragma once

#include <signal.h>

#include <vector>

#include "parser.hpp"
#include "config.hpp"
#include "PacketQueue.hpp"
#include "Logger.hpp"

struct DispatcherArgs
{
    Parser::Host host;
    std::vector<PacketQueue<char*>*>& queues;
};

struct ReceiverArgs
{
    Logger& logger;
    PerformanceConfig config;
    PacketQueue<char*>* queue;
    uint64_t id;
    uint32_t numberOfMessagesToBeSent;
};

struct SenderArgs
{
    Logger& logger;
    PerformanceConfig config;
    uint64_t src;
    Parser::Host dest;
    uint32_t numberOfMessagesToBeSent;
};

inline void assign_handler(void (*handler)(int))
{
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);
}

void* dispatch(void* ptr);
void* receive(void* ptr);
void* send(void* ptr);