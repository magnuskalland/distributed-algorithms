#pragma once

#include <mutex>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>

#include <stdio.h>

#define SENDER_FORMAT       "b %d\n"
#define RECEIVER_FORMAT     "d %ld %d\n"

class Logger
{
private:
    pthread_mutex_t logMutex, writeMutex;
    std::stringstream buffer;
public:
    Logger()
    {
        pthread_mutex_init(&logMutex, NULL);
        pthread_mutex_init(&writeMutex, NULL);
    }
    void log(std::string s)
    {
        pthread_mutex_lock(&logMutex);
        std::cout << s;
        buffer << s;
        pthread_mutex_unlock(&logMutex);
    }
    void write(std::string filePath)
    {
        std::string log = buffer.str();
        pthread_mutex_lock(&writeMutex);
        std::ofstream file(filePath);
        file << log;
        file.close();
        pthread_mutex_unlock(&writeMutex);
    }
};