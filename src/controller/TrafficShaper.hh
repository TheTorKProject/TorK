#ifndef TRAFFIC_SHAPER_HH
#define TRAFFIC_SHAPER_HH

#include "Controller.hh"
#include <thread>
#include <chrono>
#include <random>

#define TS_STRATEGY_CONSTANT    (1)
#define TS_STRATEGY_EXPONENT    (2)

#define TS_STRATEGY_ERR         (-1)
#define TS_VALID_STRATEGY(s)    (s == TS_STRATEGY_CONSTANT || \
                                 s == TS_STRATEGY_EXPONENT)

#define TS_STATE_OFF       (0)
#define TS_STATE_ON        (1)
#define TS_STATE_IDLE      (2)
#define TS_STATE_SHUTTING  (3)

class TrafficShaper {

    public:
        TrafficShaper();

        virtual ~TrafficShaper(){};

        int initialize(Controller *controller, int rate_microsec, int strategy,
                              int init_state, int run_mode);

        int terminate();

        void idle();
        void on();

        void main_thread();

        void setRate(int rate_microssec);

        int getRate();

        int getState();

    private:
        Controller* _controller = nullptr;

        int _rate_microsec;

        int _strategy;

        std::default_random_engine _generator;

        std::exponential_distribution<double> _dist_expo;

        int _state;
        std::mutex _mtx;
        std::condition_variable _cv;
};

#endif /* TRAFFIC_SHAPER_HH */
