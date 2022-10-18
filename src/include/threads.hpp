#pragma once

#include <signal.h>

#include <vector>

#include "parser.hpp"
#include "config.hpp"
#include "ReceiverWindow.hpp"
#include "PacketQueue.hpp"
#include "Logger.hpp"

inline void assign_handler(void (*handler)(int))
{
    struct sigaction sa;
    bzero(&sa, sizeof(struct sigaction));
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);
}

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
    int* sockfd;
};

struct SenderArgs
{
    Logger& logger;
    PerformanceConfig config;
    uint64_t src;
    Parser::Host dest;
    uint32_t numberOfMessagesToBeSent;
};

void* dispatch(void* ptr);
void* receive(void* ptr);
void* send(void* ptr);