#ifndef RINGBUFFER_HH
#define RINGBUFFER_HH

#include <memory>
#include <mutex>
#include <condition_variable>


#define RINGBUFFER_ENABLED          1
#define RINGBUFFER_DISABLED         2

#define RINGBUFFER_STATUS_OK        0
#define RINGBUFFER_STATUS_DISABLED  -1


template <typename T>
class RingBuffer {

public:

    RingBuffer(int max_size);

    ~RingBuffer();

    void enable();

    void disable();

    bool isEnabled();

    void put(T item, int* status);

    T get(int* status);

private:

    std::unique_ptr<T[]> buffer_;

    int head_ = 0;

    int tail_ = 0;

    int max_size_;

    int state_;

    std::mutex mtx_;
    
    std::condition_variable cv_producers_;

    std::condition_variable cv_consumers_;

};


#endif //RINGBUFFER_HH