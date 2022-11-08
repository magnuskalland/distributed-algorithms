#pragma once

#include <deque>
#include <condition_variable>

#include "packets.hpp"

template <typename T>
class PacketQueue
{
private:
    std::deque<T> dq;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
public:
    PacketQueue()
    {
        pthread_cond_init(&cond, NULL);
        pthread_mutex_init(&mutex, NULL);
    }
    ~PacketQueue()
    {
        /* Should be specific for type T -      */
        /* this won't work for types with heap  */
        /* allocated members                    */
        T back;
        while (dq.size() > 0)
        {
            back = dq.back();
            dq.pop_back();
            free(back);
        }
    }
    void pushMessage(T t)
    {
        pthread_mutex_lock(&mutex);
        dq.push_front(t);
        pthread_mutex_unlock(&mutex);
        pthread_cond_broadcast(&cond);
    }
    T popMessage()
    {
        pthread_mutex_lock(&mutex);
        if (dq.size() == 0) /* only one consumer, this if should be fine */
            pthread_cond_wait(&cond, &mutex);
        T back = dq.back();
        dq.pop_back();
        pthread_mutex_unlock(&mutex);
        return back;
    }
};