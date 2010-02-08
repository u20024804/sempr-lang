#ifndef CONDITION_HPP_INCLUDED
#define CONDITION_HPP_INCLUDED

#include <pthread.h>

#include "throwable.hpp"
#include "lock.hpp"

namespace cerl
{

    class condition
    {
    public:
        condition(mutex& mutex_) :
            _mutex(mutex_),
            _cond()
        {
            int error = ::pthread_cond_init(&_cond, NULL);
            if(error != 0)
            {
                throw exception();
            }
        }

        void wait()
        {
            int error = ::pthread_cond_wait(&_cond, &_mutex._p_mutex);
            if(error != 0)
            {
                throw exception();
            }
        }

        void notify()
        {
            int error = ::pthread_cond_signal(&_cond);
            if(error != 0)
            {
                throw exception();
            }
        }

        void notify_all()
        {
            int error = ::pthread_cond_broadcast(&_cond);
            if(error  != 0)
            {
                throw exception();
            }
        }

        ~condition()
        {
            int error =  ::pthread_cond_destroy(&_cond);
             if(error != 0)
            {
                throw exception();
            }
        }

    private:
        mutex& _mutex;
        pthread_cond_t _cond;
    };

};

#endif // CONDITION_HPP_INCLUDED
