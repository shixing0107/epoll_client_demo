#include "ringqueuepacket.h"
/*
template <class T>
RingQueuePacket<T>::RingQueuePacket(int cap)
    : ring_queue_(cap)
    , cap_(cap)
    , cur_count_(0)
{
    sem_init(&blank_sem_, 0, cap);
    sem_init(&data_sem_, 0, 0);
    c_step_ = p_step_ = 0;
}

template <class T>
RingQueuePacket<T>::~RingQueuePacket()
{
    sem_destroy(&blank_sem_);
    sem_destroy(&data_sem_);
}

template <class T>
int RingQueuePacket<T>::Push(const T& in)
{
    int ret = -1;
    //生产接口
    sem_wait(&blank_sem_);          //p空位置

    {
        std::lock_guard locker(que_mtx_);
        ring_queue_[p_step_] = in;
        p_step_++;
        p_step_ %= cap_;
        cur_count_++;
    }

    sem_post(&data_sem_);           // v数据

    return ret;
}

template <class T>
int RingQueuePacket<T>::Pop(T* out)
{
    int ret = -1;
    if (out == NULL)
        return ret;

    //消费接口
    ret = sem_trywait(&data_sem_);           // p数据
    if (ret == EAGAIN)
    {
        return ret;
    }

    {
        std::lock_guard locker(que_mtx_);
        *out = ring_queue_[c_step_];
        c_step_++;
        c_step_ %= cap_;
        cur_count_--;
    }
    sem_post(&blank_sem_);                  // v空位置

    return ret;
}

template <class T>
bool RingQueuePacket<T>::isEmpty()
{
    return cur_count_ == 0 ? true :false;
}

template <class T>
bool RingQueuePacket<T>::isFull()
{
    return cur_count_ == cap_ ? true :false;
}
*/
