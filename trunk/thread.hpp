#ifndef THREAD_HPP_INCLUDED
#define THREAD_HPP_INCLUDED

#include <time.h>
#include <pthread.h>

#include "throwable.hpp"

namespace cerl
{

    class thread
    {
    public:
        thread(void (*handle_)(void)) :
            _func_ptr0(handle_),
            _func_ptr1(NULL),
            _arg(NULL),
            _inherit(false),
            _p_thread_t(),
            _started(false)
        {
        }

        thread(void (*handle_)(void *), void *arg_) :
            _func_ptr0(),
            _func_ptr1(handle_),
            _arg(arg_),
            _inherit(false),
            _p_thread_t(),
            _started(false)
        {
        }

        thread() :
            _func_ptr0(NULL),
            _func_ptr1(NULL),
            _arg(NULL),
            _inherit(true),
            _p_thread_t(),
            _started(false)
        {
        }

        void wrapped_run()
        {
            run();
        }
        virtual void run() {}

        void start()
        {
            int error = ::pthread_create(&_p_thread_t, NULL, (void* (*)(void *))start_routine, this);
            if(error != 0)
            {
                throw exception();
            }
            _started = true;
        }

        bool started()
        {
            return _started;
        }

        void join()
        {
            int error = ::pthread_join(_p_thread_t, NULL);
            if(error != 0)
            {
                throw exception();
            }
        }

        void detach()
        {
            int error = ::pthread_detach(_p_thread_t);
            if(error != 0)
            {
                throw exception();
            }
        }

        void cancel()
        {
            int error = ::pthread_cancel(_p_thread_t);
            if(error != 0)
            {
                throw exception();
            }
        }

        virtual ~thread() {}

protected:
        void exit(void *retval)
        {
            ::pthread_exit(retval);
        }

        void testcancel()
        {
            ::pthread_testcancel();
        }

        void sleep(double sleep_time)
        {
            struct timespec timespec_;
            timespec_.tv_sec = (long)sleep_time;
            timespec_.tv_nsec = (long)((sleep_time - timespec_.tv_sec) * 1e9);
            if(nanosleep(&timespec_, NULL) != 0)
            {
                throw exception();
            }
        }

    private:
        static void* start_routine(thread * const pthread)
        {
            if(pthread->_inherit)
            {
                pthread->wrapped_run();
            }
            else if(pthread->_func_ptr0)
            {
                pthread->_func_ptr0();
            }
            else if(pthread->_func_ptr1)
            {
                pthread->_func_ptr1(pthread->_arg);
            }
            else
            {
                throw exception();
            }
            return NULL;
        }
/*
        void kill(int signo)
        {
            int error = ::pthread_kill(_p_thread_t, signo);
            if(error != 0)
            {
                throw exception();
            }
        }
*/
        void (*_func_ptr0)(void);
        void (*_func_ptr1)(void *);
        void *_arg;
        bool _inherit;
        pthread_t _p_thread_t;
        bool _started;
    };

}

#endif // THREAD_HPP_INCLUDED
