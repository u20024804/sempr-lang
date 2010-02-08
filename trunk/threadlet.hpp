#ifndef THREADLET_HPP_INCLUDED
#define THREADLET_HPP_INCLUDED

#include <ucontext.h>

#include "thread.hpp"
#include "tasklet.hpp"

namespace cerl
{
    class tasklet_service;

    class threadlet : thread
    {
    public:
        threadlet() :
            _ptasklet_service(NULL),
            _ucontext(),
            _plast_finished_tasklet(NULL),
            _pcurrent_tasklet(NULL),
            _stop(false),
            _pmutex(NULL),
            _locked(false)
        {
        }

        virtual ~threadlet()
        {
        }

        virtual void run();

        bool stopped()
        {
            return _stop;
        }

        void stop(bool stop_=true)
        {
            _stop = stop_;
        }

        tasklet& current_tasklet()
        {
            return *_pcurrent_tasklet;
        }

        void lock()
        {
            if(!_locked)
            {
                _locked = true;
                _pmutex->lock();
            }
        }

        void unlock()
        {
            if(_locked)
            {
                _pmutex->unlock();
                _locked = false;
            }
        }

    protected:
        friend class tasklet_service;

        void loop();

    private:
        tasklet_service* _ptasklet_service;
        ucontext_t _ucontext;
        tasklet *_plast_finished_tasklet;
        tasklet *_pcurrent_tasklet;
        bool _stop;
        mutex* _pmutex;
        bool _locked;

        void switch_tasklet(tasklet& tasklet_);
    };

    typedef quick_lock<threadlet> threadlet_lock;

} //namespace cerl

#endif // THREADLET_HPP_INCLUDED
