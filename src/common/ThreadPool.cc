#include "ThreadPool.hh"

ThreadPool::ThreadPool() : _stop(false) {}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> res_lock(_mtx);
        _stop = true;
    }    
    _cv.notify_all();


    for (std::thread &thread : _threads) {
        thread.join();
    }
}

int ThreadPool::initialize(int n_threads) {
    assert(n_threads > 0);

    for (int i = 0; i < n_threads; i++) {
        _threads.emplace_back(&ThreadPool::doWork, this);
    }
    return 0;
}

void ThreadPool::doWork() {
    std::function<void()> task;

    while(true) {

        {
            std::unique_lock<std::mutex> res_lock(_mtx);
            while (!_stop && _task_queue.empty()) {
                _cv.wait(res_lock);
            }

            if (_stop) {
                return;
            }

            task = _task_queue.front();
            _task_queue.pop();
        }

        task();
    }

    
}

/*template<class F, class... Args>
std::future<F> ThreadPool::submit(F&& f, Args&&... args) {

    auto task = std::make_shared<std::packaged_task<F()> >(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<F> res = task->get_future();

    {
        std::unique_lock<std::mutex> lock(_mtx);

        _task_queue.emplace([task]() { (*(task))(); });
    }
    
    _cv.notify_one();
    return res;
}*/

std::future<int> ThreadPool::submit(int (*f)(int), int args...) {

    auto task = std::make_shared<std::packaged_task<int()> >(
        std::bind(f, args)
    );

    std::future<int> res = task->get_future();

    {
        std::unique_lock<std::mutex> lock(_mtx);
        
        _task_queue.emplace([task]() { (*(task))(); });
    }
    
    _cv.notify_one();
    return res;
}

std::future<int> ThreadPool::submit(std::function<int()> function) {

    auto task = std::make_shared<std::packaged_task<int()> >(
        function
    );

    std::future<int> res = task->get_future();

    {
        std::unique_lock<std::mutex> lock(_mtx);
        
        _task_queue.emplace([task]() { (*(task))(); });
    }
    
    _cv.notify_one();
    return res;
}