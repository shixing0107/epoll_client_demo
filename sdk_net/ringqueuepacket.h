#ifndef RINGQUEUEPACKET_H
#define RINGQUEUEPACKET_H
#include <vector>
#include <semaphore.h>
#include <pthread.h>
#include <mutex>


static const int g_cap_default = 10;


template <class T>
class RingQueuePacket
{
public:
    RingQueuePacket(int cap = g_cap_default)
        : ring_queue_(cap)
        , cap_(cap)
        , cur_count_(0)
    {
        sem_init(&blank_sem_, 0, cap);
        sem_init(&data_sem_, 0, 0);
        c_step_ = p_step_ = 0;
    }



    ~RingQueuePacket()
    {
        sem_destroy(&blank_sem_);
        sem_destroy(&data_sem_);
    }

public:
    int Push(const T& in)
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


    int Pop(T* out)
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

    bool isEmpty()
    {
        return cur_count_ == 0 ? true :false;
    }

    bool isFull()
    {
        return cur_count_ == cap_ ? true :false;
    }

private:
    std::vector<T> ring_queue_;
    int cap_;
    //生产者关心空位置资源
    sem_t blank_sem_;
    //消费者关心数据
    sem_t data_sem_;
    int c_step_;
    int p_step_;
    pthread_mutex_t c_mtx_;
    pthread_mutex_t p_mtx_;
    std::mutex que_mtx_;
    int cur_count_;
};

#endif // RINGQUEUEPACKET_H
