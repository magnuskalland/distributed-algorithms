#pragma once

#include <sys/types.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include "macros.hpp"

template<typename T>
class CircularBuffer
{
public:
    inline CircularBuffer(uint32_t bufferSize)
    {
        this->bufferSize = bufferSize;
        start = 0;
        buffer = static_cast<T*>(malloc(sizeof(T) * (bufferSize)));
        if (!buffer)
        {
            traceerror();
            perror("malloc");
            return;
        }
        bzero(buffer, sizeof(T) * (bufferSize));
    }
    inline ~CircularBuffer()
    {
        free(buffer);
    }

    inline T pop()
    {
        T popped = remove(getStart());
        if (popped) shift(1);
        return popped;
    }

    /* used for traversal. Using the first sequence number of window */
    inline uint32_t getStart()
    {
        return start + 1;
    }

    /* used for traversal. Using the last sequence number of window */
    inline uint32_t getEnd()
    {
        return getStart() + bufferSize;
    }

    inline void insert(T ptr, uint32_t index)
    {
        buffer[getIndex(index - 1)] = ptr;
    }

    inline T remove(uint32_t i)
    {
        T returnPointer = buffer[getIndex(i - 1)];
        buffer[getIndex(i - 1)] = NULL;
        return returnPointer;
    }

    inline T get(uint32_t i)
    {
        return buffer[getIndex(i - 1)];
    }

    inline void shift(uint32_t count)
    {
        start += count;
    }

    inline uint32_t getShiftCounterWithPred(T pred)
    {
        uint32_t counter = 0;
        for (uint32_t i = getStart(); i < getEnd(); i++)
        {
            if (!(get(i) == pred))
            {
                return counter;
            }
            counter++;
        }
        return counter;
    }

    inline uint32_t getShiftCounter()
    {
        uint32_t counter = 0;
        for (uint32_t i = getStart(); i < getEnd(); i++)
        {
            if (!get(i))
            {
                return counter;
            }
            counter++;
        }
        return counter;
    }

    inline void print()
    {
        printf("[");
        for (uint32_t i = getStart(); i < getEnd(); i++)
        {
            printf("%d", get(i) ? i : -1);
            if (i < getEnd() - 1) printf(" ");
        }
        printf("]\n");
    }

    inline void clear(T clearValue)
    {
        for (uint32_t i = 0; i < bufferSize; i++)
        {
            buffer[i] = clearValue;
        }
    }

    inline void forceShift(uint32_t resetNumber)
    {
        start = resetNumber;
    }

private:
    inline uint32_t getIndex(uint32_t i)
    {
        return i % bufferSize;
    }

    uint32_t bufferSize;
    uint32_t start;
    T* buffer;
};