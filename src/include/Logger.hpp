#pragma once

#include <mutex>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>

#define LOG_MSG_SIZE 20

class Logger
{
private:
    std::mutex logMutex, writeMutex;
    std::stringstream buffer;
public:
    void log(std::string s)
    {
        std::cout << s; // TODO: remove
        logMutex.lock();
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