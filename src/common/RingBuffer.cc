#include "RingBuffer.hh"

#include <iostream>
#include <set>
#include <map>
#include <cassert>


template <typename T>
RingBuffer<T>::RingBuffer(int max_size):
    buffer_(std::unique_ptr<T[]>(new T[max_size])),
    max_size_(max_size),
    state_(RINGBUFFER_DISABLED)
{
    assert(max_size > 1);
};


template <typename T>
RingBuffer<T>::~RingBuffer<T>()
{
}


template <typename T>
void RingBuffer<T>::enable()
{
    std::unique_lock<std::mutex> res_lock(mtx_);
    if (state_ == RINGBUFFER_ENABLED) {
        return;
    }
    state_ = RINGBUFFER_ENABLED;
}


template <typename T>
void RingBuffer<T>::disable()
{
    std::unique_lock<std::mutex> res_lock(mtx_);
    if (state_ == RINGBUFFER_DISABLED) {
        return;
    }
    head_ = 0;
    tail_ = 0;
    buffer_ = {};
    state_ = RINGBUFFER_DISABLED;
    cv_consumers_.notify_all();
    cv_producers_.notify_all();
}


template <typename T>
bool RingBuffer<T>::isEnabled()
{
    std::unique_lock<std::mutex> res_lock(mtx_);
    return (state_ == RINGBUFFER_ENABLED);
}


template <typename T>
void RingBuffer<T>::put(T item, int* status)
{
    assert(status != NULL);
    std::unique_lock<std::mutex> res_lock(mtx_);
    while ((tail_ == (head_ - 1) % max_size_) and (state_ == RINGBUFFER_ENABLED)) {
        cv_producers_.wait(res_lock);
    }
    if (state_ == RINGBUFFER_DISABLED) {
        *status = RINGBUFFER_STATUS_DISABLED;
        return;
    }

    buffer_[tail_] = item;
    tail_ = (tail_ + 1) % max_size_;

    cv_consumers_.notify_one();

    *status = RINGBUFFER_STATUS_OK;
}


template <typename T>
T RingBuffer<T>::get(int* status)
{
    assert(status != NULL);
    std::unique_lock<std::mutex> res_lock(mtx_);

    while ((head_ == tail_) and (state_ == RINGBUFFER_ENABLED)) {
        cv_consumers_.wait(res_lock);
    }
    if (state_ == RINGBUFFER_DISABLED) {
        *status = RINGBUFFER_STATUS_DISABLED;
        return {};
    }

    T item = buffer_[head_];
    buffer_[head_] = {};
    head_ = (head_ + 1) % max_size_;

    cv_producers_.notify_one();

    *status = RINGBUFFER_STATUS_OK;
    return item;
}

template class RingBuffer<int>;
template class RingBuffer<std::string>;
