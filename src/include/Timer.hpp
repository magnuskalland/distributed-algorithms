#pragma once
#include <sys/timerfd.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "CircularBuffer.hpp"

enum type
{
    ARM, DISARM
};

class Timer
{
public:
    Timer(uint32_t* windowSize, time_t secs, long nanoSecs)
    {
        this->windowSize = windowSize;
        this->secs = secs;
        this->nanoSecs = nanoSecs;
        initialWindowSize = *windowSize;

        timerfds = new CircularBuffer<int*>(windowSize);
        timers = new CircularBuffer<struct itimerspec*>(windowSize);

        struct itimerspec* tmp_itimerspec;
        int* tmp_fd;

        for (uint32_t i = 0; i < *windowSize; i++)
        {
            tmp_itimerspec = reinterpret_cast<struct itimerspec*>(malloc(*windowSize * sizeof(struct itimerspec)));
            if (!tmp_itimerspec)
            {
                perror("malloc");
                return;
            }

            tmp_itimerspec->it_interval.tv_sec = 0;
            tmp_itimerspec->it_interval.tv_nsec = 0;
            timers->insert(tmp_itimerspec, i + 1);

            tmp_fd = reinterpret_cast<int*>(malloc(*windowSize * sizeof(int)));
            *tmp_fd = timerfd_create(CLOCK_REALTIME, 0);
            if (*tmp_fd == -1)
            {
                perror("timerfd_create");
                return;
            }
            timerfds->insert(tmp_fd, i + 1);
        }
    }
    ~Timer()
    {
        uint32_t start = getStart();
        for (uint32_t i = start; i < start + initialWindowSize; i++)
        {
            close(*timerfds->get(i));
        }
        free(timers);
        free(timerfds);
    }

    inline void shift(uint32_t shift)
    {
        timerfds->shift(shift);
        timers->shift(shift);
    }

    inline int armTimer(uint32_t sequenceNumber, type type)
    {
        int wc;
        struct itimerspec* itimerspec;
        time_t s = type == ARM ? secs : 0;
        long n = type == ARM ? nanoSecs : 0;

        itimerspec = timers->get(sequenceNumber);

        itimerspec->it_value.tv_sec = s;
        itimerspec->it_value.tv_nsec = n;

        wc = timerfd_settime(*timerfds->get(sequenceNumber), 0, itimerspec, NULL);
        if (wc == -1)
        {
            perror("timerfd_settime");
        }
        return wc;
    }

    inline uint32_t matchTimer(int fd)
    {
        for (uint32_t i = getStart(); i < getEnd(); i++)
        {
            if (fd == getTimerFd(i))
            {
                return i;
            }
        }
        return 0;
    }

    inline int getTimerFd(uint32_t sequenceNumber)
    {
        return *timerfds->get(sequenceNumber);
    }

    inline void printAll()
    {
        for (uint32_t i = getStart(); i < getEnd(); i++)
        {
            print(i);
        }
    }

    inline void print(uint32_t sequenceNumber)
    {
        printf("Timer %d (fd%d): %s\n", sequenceNumber,
            *timerfds->get(sequenceNumber),
            timers->get(sequenceNumber)->it_value.tv_sec +
            timers->get(sequenceNumber)->it_value.tv_nsec == 0
            ? "Not armed" : "Armed");
    }

private:
    inline uint32_t getStart()
    {
        return timerfds->getStart();
    }
    inline uint32_t getEnd()
    {
        return timerfds->getEnd();
    }

    uint32_t* windowSize;
    uint32_t initialWindowSize;

    time_t secs;
    long nanoSecs;

    CircularBuffer<int*>* timerfds;
    CircularBuffer<struct itimerspec*>* timers;
};