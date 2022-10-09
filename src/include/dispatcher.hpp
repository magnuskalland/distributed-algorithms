#pragma once

#include <vector>

#include "parser.hpp"
#include "PacketQueue.hpp"

int dispatch(const Parser::Host host, std::vector<PacketQueue<char*>*>& queues);