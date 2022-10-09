#pragma once

#include <sys/types.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>

template<typename T>
class CircularBuffer
{
public:
    CircularBuffer(uint32_t* bufferSize)
    {
        this->bufferSize = bufferSize;
        initialBufferSize = *bufferSize;
        start = 0;
        buffer = static_cast<T*>(malloc(sizeof(T) * (*bufferSize)));
        if (!buffer)
        {
            perror("malloc");
            return;
        }
    }
    ~CircularBuffer()
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
        return getStart() + *bufferSize;
    }

    inline void insert(T ptr, uint32_t i)
    {
        buffer[getIndex(i - 1)] = ptr;
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

    inline uint32_t getShiftCounter()
    {
        uint32_t counter = 0;
        for (uint32_t i = getStart(); i < getEnd(); i++)
        {
            if (get(i) == NULL)
            {
                return counter;
            }
            counter++;
        }
        return counter;
    }

    inline void print()
    {
        for (uint32_t i = getStart(); i < getEnd(); i++)
        {
            printf("%d ", get(i) ? get(i)->seqnr : 0);
        }
        printf("\n");
    }

private:
    inline uint32_t getIndex(uint32_t i)
    {
        return i % *bufferSize;
    }
    uint32_t initialBufferSize;
    uint32_t* bufferSize;
    uint32_t start;
    T* buffer;
};