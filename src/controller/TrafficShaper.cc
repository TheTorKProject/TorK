#include "TrafficShaper.hh"
#include "../common/Common.hh"

TrafficShaper::TrafficShaper() {}

int TrafficShaper::initialize(Controller *controller,
                              int rate_microsec, int strategy, int init_state,
                              int run_mode) {

    assert(controller != nullptr);
    _controller = controller;

    assert(rate_microsec > 0);
    _rate_microsec = rate_microsec;

    assert(TS_VALID_STRATEGY(strategy));
    _strategy = strategy;

    _state = init_state;

    _dist_expo = std::exponential_distribution<double>(
                    (double)20/(double)_rate_microsec);

    if (run_mode == RUN_BACKGROUND) {
        std::thread th(&TrafficShaper::main_thread, this);
        th.detach();
        return 0;
    }

    if (run_mode == RUN_FOREGROUND) {
        main_thread();
        return 0;
    }

    return -1;
}

int TrafficShaper::terminate() {
    {
        std::unique_lock<std::mutex> res_lock(_mtx);
        _state = TS_STATE_SHUTTING;
    }
    _cv.notify_one();

    {
        std::unique_lock<std::mutex> res_lock(_mtx);
        // properly terminate background thread
        while(_state != TS_STATE_OFF) {
            _cv.wait(res_lock);
        }
    }
    return 0;
}

void TrafficShaper::idle() {
    std::unique_lock<std::mutex> res_lock(_mtx);
    _state = TS_STATE_IDLE;
}

void TrafficShaper::on() {
    {
        std::unique_lock<std::mutex> res_lock(_mtx);
        _state = TS_STATE_ON;
    }
    _cv.notify_one();
}

void TrafficShaper::main_thread()
{
    assert(_controller != nullptr);
    int rate, adj_rate;
    std::chrono::high_resolution_clock::time_point start, end;
    std::chrono::duration<double> diff;

    switch (_strategy) {
        case TS_STRATEGY_CONSTANT:
            while(true) {
                {
                    std::unique_lock<std::mutex> res_lock(_mtx);
                    if (_state == TS_STATE_SHUTTING) {
                        _state = TS_STATE_OFF;
                        break;
                    }

                    while(_state == TS_STATE_IDLE) {
                        _cv.wait(res_lock);
                    }
                    rate = _rate_microsec;
                }
                start = std::chrono::high_resolution_clock::now();
                _controller->handleTrafficShapingEvent();
                end = std::chrono::high_resolution_clock::now();

                diff = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                adj_rate = abs(rate - diff.count());
                std::this_thread::sleep_for(std::chrono::microseconds(adj_rate));
            }
        break;
        case TS_STRATEGY_EXPONENT:
            while(true) {
                {
                    std::unique_lock<std::mutex> res_lock(_mtx);
                    if (_state == TS_STATE_SHUTTING) {
                        _state = TS_STATE_OFF;
                        break;
                    }

                    while(_state == TS_STATE_IDLE) {
                        _cv.wait(res_lock);
                    }
                    rate = _rate_microsec;
                }
                start = std::chrono::high_resolution_clock::now();
                _controller->handleTrafficShapingEvent();
                end = std::chrono::high_resolution_clock::now();

                diff = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                adj_rate = abs(rate - diff.count());
                std::this_thread::sleep_for(std::chrono::microseconds(
                    std::lround(_dist_expo(_generator) + adj_rate)));

            }
        break;
    }

    _cv.notify_all();
}

void TrafficShaper::setRate(int rate_microssec) {
    std::unique_lock<std::mutex> res_lock(_mtx);
    _rate_microsec = rate_microssec;
}

int TrafficShaper::getRate() {
    std::unique_lock<std::mutex> res_lock(_mtx);
    return _rate_microsec;
}

int TrafficShaper::getState() {
    std::unique_lock<std::mutex> res_lock(_mtx);
    return _state;
}
