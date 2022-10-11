#ifndef THREAD_POOL_HH
#define THREAD_POOL_HH

#include "Common.hh"
#include <thread>
#include <future>
#include <queue>
#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>

class ThreadPool {

    public:
        ThreadPool();

        ~ThreadPool();

        int initialize(int n_threads);

        /*template<class F, class... Args>
        std::future<F> submit(F&& f, Args&&... args);*/

        std::future<int> submit(int (*f)(int), int args...);
        std::future<int> submit(std::function<int()> function);

    private:
        std::vector<std::thread> _threads;

        std::queue<std::function<void()> > _task_queue;

        std::mutex _mtx;

        std::condition_variable _cv;

        bool _stop;

        void doWork();

};

#endif /* THREAD_POOL_HH */