#include <ucontext.h>

#include "threadlet.hpp"
#include "tasklet_service.hpp"

namespace cerl
{

    void threadlet::run()
    {
        try
        {
            while (!_stop)
            {
                loop();
            }
        }
        catch (stop_exception& e)
        {
        }
        if (_plast_finished_tasklet != NULL)
        {
            delete _plast_finished_tasklet;
            _plast_finished_tasklet = NULL;
        }
    }

    void threadlet::loop()
    {
        if (_plast_finished_tasklet != NULL)
        {
            delete _plast_finished_tasklet;
            _plast_finished_tasklet = NULL;
        }
        threadlet_lock lock(this);
        _pcurrent_tasklet = &_ptasklet_service->pop_tasklet();
        if (_stop)
        {
            lock.unlock();
            assert(_pcurrent_tasklet != NULL);
            delete _pcurrent_tasklet;
            throw stop_exception();
        }
        switch_tasklet(*_pcurrent_tasklet);
    }

    void threadlet::switch_tasklet(tasklet& tasklet_)
    {
        tasklet_._pthreadlet = this;
        swapcontext(&_ucontext, &tasklet_._ucontext);
    }

}
