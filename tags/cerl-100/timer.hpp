#ifndef TIMER_HPP_INCLUDED
#define TIMER_HPP_INCLUDED

#include <sys/time.h>

#include "constant.hpp"
#include "thread.hpp"
#include "tasklet_service.hpp"

namespace cerl
{

    class timer : public thread
    {
    public:
        timer(tasklet_service& tasklet_service_, double interval_=DEFAULT_INTERVAL) :
                _tasklet_service(tasklet_service_),
                _time(0),
                _interval(interval_),
                _stop(false)
        {
            timeval now;
            if(gettimeofday(&now, NULL) != 0)
            {
                throw exception(__FILE__, __LINE__);
            }
            _time = now.tv_sec * 10 + now.tv_usec * 1e-5;
        }

        void run();

        bool stopped()
        {
            return _stop;
        }

        void stop(bool stop_=true)
        {
            _stop = stop_;
        }

        unsigned long long time()
        {
            return _time;
        }
    protected:
        tasklet_service& _tasklet_service;
        unsigned long long _time;
        double _interval;
        void loop();
    private:
        bool _stop;
    };

} //namespace cerl

#endif // TIMER_HPP_INCLUDED
