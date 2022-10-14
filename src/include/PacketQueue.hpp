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
        /* TOOD: must be specific for type T    */
        /* this won't work for types with heap  */
        /* allocated members                    */
        empty();
    }
    void push_msg(T t)
    {
        dq.push_front(t);
        pthread_cond_broadcast(&cond);
    }
    T get_msg()
    {
        pthread_mutex_lock(&mutex);
        if (dq.size() == 0) /* only one consumer, should't need a while loop */
            pthread_cond_wait(&cond, &mutex);
        pthread_mutex_unlock(&mutex);
        T back = dq.back();
        dq.pop_back();
        return back;
    }
private:
    void empty()
    {
        T back;
        while (dq.size() > 0)
        {
            back = dq.back();
            dq.pop_back();
            free(back);
        }
    }
};