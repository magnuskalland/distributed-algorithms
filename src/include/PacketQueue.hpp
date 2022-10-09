#pragma once

#include <deque>
#include <condition_variable>

#include "packets.hpp"

template <typename T>
class PacketQueue
{
private:
    std::deque<T> dq;
    std::mutex mutex;
    std::condition_variable cond;
public:
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
        cond.notify_all();
    }
    T get_msg()
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (dq.size() == 0)
            cond.wait(lock);
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