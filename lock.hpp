#ifndef MUTEX_HPP_INCLUDED
#define MUTEX_HPP_INCLUDED

#include <pthread.h>
#include "throwable.hpp"

namespace cerl
{

    class mutex
    {
    public:
        inline mutex() :
                _p_mutex()
        {
            int error = ::pthread_mutex_init(&_p_mutex, 0);
            if (error != 0)
            {
                throw exception();
            }
        }

        inline void lock()
        {
            int error = ::pthread_mutex_lock(&_p_mutex);
            if (error != 0)
            {
                throw exception();
            }
        }

        inline void unlock()
        {
            int error = ::pthread_mutex_unlock(&_p_mutex);
            if (error != 0)
            {
                throw exception();
            }
        }

        inline ~mutex()
        {
            int error = ::pthread_mutex_destroy(&_p_mutex);
            if (error != 0)
            {
                throw exception();
            }
        }
    private:
        ::pthread_mutex_t _p_mutex;
        friend class condition;
    };

    class spin
    {
    public:
        inline spin() : _p_spin()
        {
            int error = ::pthread_spin_init(&_p_spin, PTHREAD_PROCESS_PRIVATE);
            if (error != 0)
            {
                throw exception();
            }
        }

        inline void lock()
        {
            int error = ::pthread_spin_lock(&_p_spin);
            if (error != 0)
            {
                throw exception();
            }
        }

        inline void unlock()
        {
            int error = ::pthread_spin_unlock(&_p_spin);
            if (error != 0)
            {
                throw exception();
            }
        }

        inline ~spin()
        {
            int error = ::pthread_spin_destroy(&_p_spin);
            if (error != 0)
            {
                throw exception();
            }
        }
    private:
        ::pthread_spinlock_t _p_spin;
    };

    template <typename M>
    class scoped_lock
    {
    public:
        inline scoped_lock(M& m) : _mutex(m)
        {
            _mutex.lock();
            _is_locked = true;
        }

        inline void lock()
        {
            if (!_is_locked)
            {
                _mutex.lock();
                _is_locked = true;
            }
        }

        inline void unlock()
        {
            if (_is_locked)
            {
                _mutex.unlock();
                _is_locked = false;
            }
        }

        inline ~scoped_lock()
        {
            if (_is_locked)
            {
                unlock();
            }
        }
    private:
        bool _is_locked;
        M& _mutex;
    };

    template<typename M>
    class quick_lock
    {
    public:
        quick_lock(M& m_) : _m(m_)
        {
            _m.lock();
        }
        quick_lock(M *pm) : _m(*pm)
        {
            _m.lock();
        }

        ~quick_lock()
        {
            _m.unlock();
        }

    private:
        M &_m;
    };

    typedef scoped_lock<mutex> mutex_lock;
    typedef scoped_lock<spin> spin_lock;

} //namespace cerl

#endif // MUTEX_HPP_INCLUDED
