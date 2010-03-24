#include "timer.hpp"

namespace cerl
{

    void timer::run()
    {
        timeval start_run;
        if (gettimeofday(&start_run, NULL) != 0)
        {
            throw exception(__FILE__, __LINE__);
        }
        _time = start_run.tv_sec * 10 + start_run.tv_usec * 1e-5;

        while (!_stop)
        {
            sleep(_interval);
            _time++;
            if (_time % 10 == 0)
            {
                timeval now;
                if (gettimeofday(&now, NULL) != 0)
                {
                    throw exception(__FILE__, __LINE__);
                }
                _time = now.tv_sec * 10 + now.tv_usec * 1e-5;
            }
            mutex_lock lock(_tasklet_service._mutex);
            loop();
        }
    }

    static message msg_no = {{}, no_msg};

    void timer::loop()
    {
        for (set<tasklet*>::iterator it = _tasklet_service._sleepers.begin();
                it != _tasklet_service._sleepers.end();)
        {
            tasklet &tasklet_ = **it;
            if (tasklet_._wake > _time)
            {
                return;
            }
            set<tasklet*>::iterator next = it;
            ++next;
            _tasklet_service._sleepers.erase(it);
            it = next;
            if (tasklet_.state() == timeout_waitting)
            {
                _tasklet_service._channel_manager.dispatch(tasklet_._channel, msg_no, false);
            }
            else
            {
                _tasklet_service.push_tasklet(tasklet_);
            }
        }
    }

} //namespace cerl
