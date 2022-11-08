#pragma once

#include <mutex>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>

#define SENDER_FORMAT       "b %d\n"
#define RECEIVER_FORMAT     "d %ld %d\n"

class Logger
{
private:
    std::mutex logMutex, writeMutex;
    std::stringstream buffer;
public:
    void log(std::string s)
    {
        logMutex.lock();
        std::cout << s;
        buffer << s;
        logMutex.unlock();
    }
    void write(std::string filePath)
    {
        std::string log = buffer.str();
        std::ofstream file(filePath);
        writeMutex.lock();
        file << log;
        writeMutex.unlock();
        file.close();
    }
};